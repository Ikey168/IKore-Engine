#pragma once

#include "core/ecs/ECS.h"
#include "core/ecs/components/Components.h"
#include "core/sim/Crowd.h"
#include "core/sim/Simulation.h"
#include "core/sim/Timeline.h"
#include "world/GeoJsonImporter.h"
#include "world/NavMesh.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

/**
 * @file VerticalSlice.h
 * @brief The roadmap's headline vertical slice, composed end to end (EXPANSION_IDEAS 6).
 *
 * "Import a real neighborhood, walk it, watch a crowd navigate it, then rewind
 * time." This is the integration that ties the finished foundations into one
 * story: the GeoJSON importer (world), a baked nav grid + flow field (world/sim),
 * an archetype-ECS crowd (ecs/sim), and deterministic fixed-step time with
 * snapshot rewind (sim::Timeline).
 *
 * The pieces already exist and are individually tested; this is the glue plus a
 * small, testable state machine. It is headless (no GL): the ImGui walkthrough and
 * rewind scrubber sit on top of this in DebugUI. Because every dependency is
 * header-only and glm-free (ecs::Vec3), the whole slice is unit-testable, including
 * its determinism guarantee: replaying from a rewound tick reproduces the crowd
 * exactly. Header-only, std only.
 */
namespace IKore {
namespace game {

/// Per-agent state captured each tick so the crowd can be rewound and replayed.
struct AgentSnapshot {
    ecs::Vec3 position;
    bool arrived{false};
};
using CrowdState = std::vector<AgentSnapshot>;

class VerticalSlice {
public:
    struct Config {
        float cellSize{1.0f};            ///< Nav-grid cell size (world units).
        float agentRadius{0.3f};         ///< Obstacle inflation for baking.
        float boundsMargin{4.0f};        ///< Padding around the imported extents.
        float agentSpeed{4.0f};          ///< Crowd agent speed (units/second).
        double fixedDelta{1.0 / 60.0};   ///< Deterministic simulation step.
        int maxStepsPerFrame{8};         ///< Timeline catch-up clamp.
        std::size_t historyCapacity{600};///< Rewind window in ticks.
        std::uint64_t seed{1};           ///< Deterministic spawn scatter.
    };

    VerticalSlice() : VerticalSlice(Config{}) {}

    explicit VerticalSlice(Config config)
        : m_config(config),
          m_timeline(CrowdState{}, config.seed, config.fixedDelta, config.historyCapacity,
                     config.maxStepsPerFrame) {}

    /// Import a neighborhood from GeoJSON and bake the nav grid. Returns true if the
    /// result has any walkable space.
    bool loadFromGeoJson(const std::string& geojson, const world::GeoImportOptions& options = {}) {
        m_city = world::importString(geojson, options);
        return bakeFromCity();
    }

    /// Build directly from an already-parsed city (used by tests and tooling).
    bool buildFrom(const world::City& city) {
        m_city = city;
        return bakeFromCity();
    }

    /// Choose the crowd's destination and build the flow field toward it.
    void setGoal(const ecs::Vec3& goal) {
        m_goal = goal;
        m_field = sim::buildFlowField(m_grid, goal);
        m_hasField = true;
    }

    /**
     * @brief Spawn up to @p count agents on walkable cells, scattered deterministically.
     *
     * Cells within one cell of the goal are skipped so agents actually travel. The
     * spawn set only depends on the config seed and the grid, so runs reproduce.
     */
    std::size_t spawnCrowd(int count) {
        std::vector<ecs::Vec3> spots;
        const int goalCx = m_grid.cellX(m_goal.x), goalCz = m_grid.cellZ(m_goal.z);
        for (int cz = 0; cz < m_grid.height; ++cz) {
            for (int cx = 0; cx < m_grid.width; ++cx) {
                if (!m_grid.isWalkable(cx, cz)) continue;
                if (m_hasField && std::abs(cx - goalCx) <= 1 && std::abs(cz - goalCz) <= 1) continue;
                spots.push_back(m_grid.cellCenter(cx, cz));
            }
        }
        if (spots.empty() || count <= 0) return 0;

        sim::DeterministicRng rng(m_config.seed);
        for (int i = 0; i < count; ++i) {
            const ecs::Vec3& pos = spots[static_cast<std::size_t>(rng.nextInt(0, static_cast<int>(spots.size())))];
            m_agents.push_back(sim::spawnAgent(m_registry, pos, m_config.agentSpeed));
        }
        captureInto(m_initialState);
        return m_agents.size();
    }

    /// Advance exactly one fixed tick. Returns the number of agents still moving.
    int step() {
        if (!m_hasField) return 0;
        m_timeline.advance(m_config.fixedDelta,
                           [this](CrowdState& state, std::uint64_t, sim::DeterministicRng&) {
                               simulateStep(state);
                           });
        return m_moving;
    }

    /// Advance by real elapsed time (runs the appropriate number of fixed steps).
    int advance(double realSeconds) {
        if (!m_hasField) return 0;
        m_timeline.advance(realSeconds,
                           [this](CrowdState& state, std::uint64_t, sim::DeterministicRng&) {
                               simulateStep(state);
                           });
        return m_moving;
    }

    /// Rewind to @p tick and restore the crowd to how it was then (if in-window).
    bool rewindTo(std::uint64_t tick) {
        if (!m_timeline.rewindTo(tick)) return false;
        restoreFrom(m_timeline.state());
        return true;
    }

    /// Restore the crowd to its spawn state (does not reset the timeline clock).
    void resetCrowd() { restoreFrom(m_initialState); }

    // --- Accessors ---------------------------------------------------------
    const world::City& city() const { return m_city; }
    const world::NavGrid& grid() const { return m_grid; }
    const sim::FlowField& field() const { return m_field; }
    ecs::Registry& registry() { return m_registry; }
    ecs::Vec3 goal() const { return m_goal; }
    bool hasGoal() const { return m_hasField; }

    std::uint64_t tick() const { return m_timeline.tick(); }
    std::uint64_t oldestTick() const { return m_timeline.oldestTick(); }
    std::size_t agentCount() const { return m_agents.size(); }
    int agentsMoving() const { return m_moving; }

    ecs::Vec3 agentPosition(std::size_t i) const {
        return m_registry.get<ecs::Transform>(m_agents[i]).position;
    }
    bool agentArrived(std::size_t i) const { return m_registry.get<sim::CrowdAgent>(m_agents[i]).arrived; }

    /// Number of agents that have reached the goal.
    std::size_t arrivedCount() const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_agents.size(); ++i) {
            if (agentArrived(i)) ++n;
        }
        return n;
    }

private:
    // The timeline's per-tick step: advance the crowd, then mirror it into the
    // snapshot state so it can be rewound. steerCrowd is deterministic, so the same
    // start state always produces the same trajectory.
    void simulateStep(CrowdState& state) {
        m_moving = sim::steerCrowd(m_registry, m_grid, m_field, static_cast<float>(m_config.fixedDelta));
        captureInto(state);
    }

    void captureInto(CrowdState& state) const {
        state.resize(m_agents.size());
        for (std::size_t i = 0; i < m_agents.size(); ++i) {
            state[i].position = m_registry.get<ecs::Transform>(m_agents[i]).position;
            state[i].arrived = m_registry.get<sim::CrowdAgent>(m_agents[i]).arrived;
        }
    }

    void restoreFrom(const CrowdState& state) {
        const std::size_t n = state.size() < m_agents.size() ? state.size() : m_agents.size();
        for (std::size_t i = 0; i < n; ++i) {
            m_registry.get<ecs::Transform>(m_agents[i]).position = state[i].position;
            m_registry.get<sim::CrowdAgent>(m_agents[i]).arrived = state[i].arrived;
        }
    }

    bool bakeFromCity() {
        float minX = 0.0f, minZ = 0.0f, maxX = 0.0f, maxZ = 0.0f;
        if (!computeBounds(minX, minZ, maxX, maxZ)) return false;

        std::vector<world::Obstacle> obstacles;
        obstacles.reserve(m_city.buildings.size());
        for (const world::Building& b : m_city.buildings) {
            obstacles.push_back(world::Obstacle{b.center, b.size, 0.0f});
        }
        m_grid = world::bake(minX, minZ, maxX, maxZ, m_config.cellSize, obstacles, m_config.agentRadius);
        return m_grid.walkableCount() > 0;
    }

    bool computeBounds(float& minX, float& minZ, float& maxX, float& maxZ) const {
        bool any = false;
        const auto include = [&](const ecs::Vec3& p) {
            if (!any) {
                minX = maxX = p.x;
                minZ = maxZ = p.z;
                any = true;
            } else {
                if (p.x < minX) minX = p.x;
                if (p.x > maxX) maxX = p.x;
                if (p.z < minZ) minZ = p.z;
                if (p.z > maxZ) maxZ = p.z;
            }
        };
        for (const world::Building& b : m_city.buildings) {
            for (const ecs::Vec3& p : b.footprint) include(p);
        }
        for (const world::Road& r : m_city.roads) {
            for (const ecs::Vec3& p : r.centerline) include(p);
        }
        for (const world::Region& region : m_city.regions) {
            for (const ecs::Vec3& p : region.footprint) include(p);
        }
        if (!any) return false;
        minX -= m_config.boundsMargin;
        minZ -= m_config.boundsMargin;
        maxX += m_config.boundsMargin;
        maxZ += m_config.boundsMargin;
        return true;
    }

    Config m_config;
    world::City m_city;
    world::NavGrid m_grid;
    sim::FlowField m_field;
    ecs::Registry m_registry;
    std::vector<ecs::Entity> m_agents;
    sim::Timeline<CrowdState> m_timeline;
    CrowdState m_initialState;
    ecs::Vec3 m_goal{};
    bool m_hasField{false};
    int m_moving{0};
};

} // namespace game
} // namespace IKore
