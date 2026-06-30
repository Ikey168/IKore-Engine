// Test for the flow-field crowd simulation - Milestone 12, #153.
//
// Verifies the issue's acceptance criteria:
//   - Scale: a large crowd (2000 agents) simulates over the nav-grid. The flow
//     field makes steering O(1) per agent, so this is the interactive-rate path;
//     the test prints throughput (informational) and asserts correctness.
//   - Agents avoid obstacles: every agent stays on walkable cells for the whole
//     run (it must detour through a gap in a dividing wall).
//   - Agents reach goals without stalls: the entire crowd arrives within a
//     bounded number of steps.
// Also checks determinism: same seed -> identical final state (ties #152 + #153).
//
// Pure std + header-only ECS / NavGrid / Simulation:
//   g++ -std=c++17 -I src tests/test_crowd.cpp -o test_crowd

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"
#include "core/ecs/components/Components.h"
#include "core/sim/Crowd.h"
#include "core/sim/Simulation.h"
#include "world/NavMesh.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// A 48x48 field split by a wall at x=24 spanning z in [0,40], leaving a gap
// (door) at the top (z in [40,48]). Agents start on the left and the goal is on
// the right, so the whole crowd must route up through the gap and back down.
static world::NavGrid makeWalledGrid() {
    std::vector<world::Obstacle> obstacles = {
        {{24.0f, 0.0f, 20.0f}, {1.0f, 3.0f, 40.0f}, 0.0f}, // wall covering z ~0..40
    };
    return world::bake(0.0f, 0.0f, 48.0f, 48.0f, 1.0f, obstacles);
}

// Spawn `count` agents at deterministic random walkable cells on the left side
// (x < 24), each able to reach the goal. Returns the spawned entities.
static std::vector<ecs::Entity> spawnLeftCrowd(ecs::Registry& reg, const world::NavGrid& grid,
                                               const sim::FlowField& field, std::uint64_t seed,
                                               int count) {
    sim::DeterministicRng rng(seed);
    std::vector<ecs::Entity> agents;
    agents.reserve(static_cast<std::size_t>(count));
    while (static_cast<int>(agents.size()) < count) {
        const int cx = rng.nextInt(1, 22);
        const int cz = rng.nextInt(1, 46);
        if (!grid.isWalkable(cx, cz) || !field.reachable(cx, cz)) continue;
        const ecs::Vec3 pos = grid.cellCenter(cx, cz);
        agents.push_back(sim::spawnAgent(reg, pos, 6.0f, 0.3f));
    }
    return agents;
}

// True if every agent currently sits on a walkable cell.
static bool allOnWalkable(ecs::Registry& reg, const world::NavGrid& grid) {
    bool ok = true;
    reg.view<ecs::Transform, sim::CrowdAgent>().each(
        [&](ecs::Entity, ecs::Transform& t, sim::CrowdAgent&) {
            if (!grid.isWalkable(grid.cellX(t.position.x), grid.cellZ(t.position.z))) ok = false;
        });
    return ok;
}

static std::vector<float> finalPositions(ecs::Registry& reg) {
    std::vector<float> out;
    reg.view<ecs::Transform, sim::CrowdAgent>().each(
        [&](ecs::Entity, ecs::Transform& t, sim::CrowdAgent&) {
            out.push_back(t.position.x);
            out.push_back(t.position.z);
        });
    return out;
}

static void testFlowFieldBasics() {
    const world::NavGrid grid = makeWalledGrid();
    const ecs::Vec3 goal{46.5f, 0.0f, 2.5f}; // right side, near the bottom
    const sim::FlowField field = sim::buildFlowField(grid, goal);

    // The goal cell is reachable with zero cost; a left-side cell is reachable
    // (only via the gap) and its flow is a unit vector pointing somewhere.
    CHECK(field.reachable(grid.cellX(goal.x), grid.cellZ(goal.z)));
    CHECK(field.reachable(2, 2));
    const ecs::Vec3 dir = field.directionAt(2.5f, 2.5f);
    CHECK(std::fabs(std::sqrt(dir.x * dir.x + dir.z * dir.z) - 1.0f) < 1e-3f);

    // A cell inside the wall is blocked and unreachable, with no flow.
    CHECK(!grid.isWalkable(24, 10));
    CHECK(!field.reachable(24, 10));
}

static void testCrowdReachesGoalAvoidingObstacles() {
    const world::NavGrid grid = makeWalledGrid();
    const ecs::Vec3 goal{46.5f, 0.0f, 2.5f};
    const sim::FlowField field = sim::buildFlowField(grid, goal);

    const int kAgents = 2000;
    ecs::Registry reg;
    const std::vector<ecs::Entity> agents = spawnLeftCrowd(reg, grid, field, /*seed=*/42, kAgents);
    CHECK(static_cast<int>(agents.size()) == kAgents);
    CHECK(allOnWalkable(reg, grid)); // valid initial placement

    const float dt = 0.1f;
    const int kMaxSteps = 800;
    int steps = 0, moving = kAgents;
    bool stayedWalkable = true;

    const auto t0 = std::chrono::steady_clock::now();
    for (; steps < kMaxSteps && moving > 0; ++steps) {
        moving = sim::steerCrowd(reg, grid, field, dt);
        if (!allOnWalkable(reg, grid)) stayedWalkable = false;
    }
    const auto t1 = std::chrono::steady_clock::now();

    CHECK(stayedWalkable);   // never walked through the wall (obstacle avoidance)
    CHECK(moving == 0);      // whole crowd arrived: reached goals without stalls
    CHECK(steps < kMaxSteps);

    // Every agent is at the goal (within its arrive radius).
    int notArrived = 0;
    reg.view<ecs::Transform, sim::CrowdAgent>().each(
        [&](ecs::Entity, ecs::Transform& t, sim::CrowdAgent& a) {
            const float dx = goal.x - t.position.x, dz = goal.z - t.position.z;
            if (!a.arrived || std::sqrt(dx * dx + dz * dz) > a.arriveRadius + 1e-3f) ++notArrived;
        });
    CHECK(notArrived == 0);

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double agentSteps = static_cast<double>(kAgents) * steps;
    std::printf("[info] %d agents reached goal in %d steps, %.1f ms (%.2f M agent-steps/s)\n",
                kAgents, steps, ms, ms > 0.0 ? agentSteps / ms / 1000.0 : 0.0);
}

static void testDeterminism() {
    const world::NavGrid grid = makeWalledGrid();
    const ecs::Vec3 goal{46.5f, 0.0f, 2.5f};
    const sim::FlowField field = sim::buildFlowField(grid, goal);

    // Step only part-way: agents are still mid-route (none can reach the goal
    // through the gap this soon), so positions reflect their distinct seeded
    // starts rather than all collapsing onto the single goal point.
    auto run = [&](std::uint64_t seed) {
        ecs::Registry reg;
        spawnLeftCrowd(reg, grid, field, seed, 256);
        for (int i = 0; i < 40; ++i) sim::steerCrowd(reg, grid, field, 0.1f);
        return finalPositions(reg);
    };

    const std::vector<float> a = run(7);
    const std::vector<float> b = run(7);
    const std::vector<float> c = run(8);
    CHECK(a == b);            // same seed -> identical crowd state
    CHECK(a.size() == c.size());
    CHECK(a != c);            // different seed -> different start, different state
}

int main() {
    testFlowFieldBasics();
    testCrowdReachesGoalAvoidingObstacles();
    testDeterminism();

    if (g_failures == 0) {
        std::printf("All crowd simulation tests passed.\n");
        return 0;
    }
    std::printf("%d crowd simulation test(s) failed.\n", g_failures);
    return 1;
}
