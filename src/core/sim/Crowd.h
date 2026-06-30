#pragma once

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"
#include "core/ecs/components/Components.h"
#include "world/NavMesh.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

/**
 * @file Crowd.h
 * @brief Scalable flow-field crowd simulation over a nav-grid (issue #153).
 *
 * Milestone 12. Drives thousands of autonomous agents toward a shared goal
 * across the navigation grid baked in Milestone 11 (NavMesh.h), using the
 * data-oriented ECS from Milestone 8 (Registry/View + value components).
 *
 * The scale comes from a flow field: a single Dijkstra pass from the goal over
 * the walkable cells produces an "integration field" (cost-to-goal) and, from
 * its gradient, a per-cell flow direction. Building it is O(cells) and happens
 * once per goal; steering is then an O(1) table lookup per agent per step, so
 * 1000+ agents cost essentially the same per agent as one. This replaces a
 * per-agent A* (which would dominate at crowd scale) while reusing the same
 * walkable grid, so agents route around the static obstacles (walls/buildings)
 * the importers emit and reach the goal without getting stuck.
 *
 * Agents are point followers that may overlap (no agent-agent collision), which
 * is what guarantees "without stalls": every reachable cell's flow points
 * strictly downhill in cost toward the goal, so progress is monotonic and no
 * deadlock can form. Local agent-agent avoidance (e.g. RVO) is a future layer.
 *
 * Header-only and dependency-free (std + the header-only ECS and NavGrid), so it
 * is unit-tested in isolation.
 */
namespace IKore {
namespace sim {

/// ECS component: an agent that follows a flow field toward the goal.
struct CrowdAgent {
    float speed{4.0f};         ///< World units per second.
    float arriveRadius{0.3f};  ///< Snap to the goal and stop within this distance.
    bool arrived{false};       ///< Set once the agent has reached the goal.
};

/**
 * @brief A flow field over a NavGrid: per-cell cost-to-goal and a flow direction.
 *
 * Sampling is O(1): map a world position to its cell and read the precomputed
 * unit flow vector pointing one step downhill toward the goal.
 */
class FlowField {
public:
    float minX{0.0f}, minZ{0.0f}, cellSize{1.0f};
    int width{0}, height{0};
    ecs::Vec3 goal{};
    std::vector<float> cost;  ///< Distance-to-goal per cell; +inf if unreachable.
    std::vector<float> flowX; ///< Unit flow X per cell (0 at goal/unreachable).
    std::vector<float> flowZ; ///< Unit flow Z per cell.

    bool inBounds(int cx, int cz) const { return cx >= 0 && cz >= 0 && cx < width && cz < height; }
    int cellX(float worldX) const { return static_cast<int>(std::floor((worldX - minX) / cellSize)); }
    int cellZ(float worldZ) const { return static_cast<int>(std::floor((worldZ - minZ) / cellSize)); }

    /// True if the cell can reach the goal (finite cost).
    bool reachable(int cx, int cz) const {
        return inBounds(cx, cz) &&
               cost[static_cast<std::size_t>(cz) * width + cx] != std::numeric_limits<float>::infinity();
    }

    /// Unit flow direction at a world position (y = 0), or zero if none.
    ecs::Vec3 directionAt(float worldX, float worldZ) const {
        const int cx = cellX(worldX), cz = cellZ(worldZ);
        if (!inBounds(cx, cz)) return ecs::Vec3{};
        const std::size_t i = static_cast<std::size_t>(cz) * width + cx;
        return ecs::Vec3{flowX[i], 0.0f, flowZ[i]};
    }
};

/**
 * @brief Build a flow field over @p grid that points every walkable cell toward
 *        @p goal. 8-connected Dijkstra (octile costs, no diagonal corner-cut),
 *        matching NavMesh.h findPath, so the two agree on connectivity.
 */
inline FlowField buildFlowField(const world::NavGrid& grid, const ecs::Vec3& goal) {
    FlowField field;
    field.minX = grid.minX;
    field.minZ = grid.minZ;
    field.cellSize = grid.cellSize;
    field.width = grid.width;
    field.height = grid.height;
    field.goal = goal;

    const int w = grid.width, h = grid.height;
    const std::size_t n = static_cast<std::size_t>(w) * h;
    const float kInf = std::numeric_limits<float>::infinity();
    field.cost.assign(n, kInf);
    field.flowX.assign(n, 0.0f);
    field.flowZ.assign(n, 0.0f);
    if (n == 0) return field;

    const int gx = grid.cellX(goal.x), gz = grid.cellZ(goal.z);
    if (!grid.isWalkable(gx, gz)) return field; // goal blocked: empty (all unreachable)

    const auto idx = [w](int x, int z) { return static_cast<std::size_t>(z) * w + x; };
    const float kSqrt2 = 1.41421356f;
    const int dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dzs[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    // Corner-cut-safe walkable neighbour test (shared by the search and gradient).
    const auto canStep = [&](int cx, int cz, int d) {
        const int nx = cx + dxs[d], nz = cz + dzs[d];
        if (!grid.isWalkable(nx, nz)) return false;
        if (dxs[d] != 0 && dzs[d] != 0) {
            if (!grid.isWalkable(cx + dxs[d], cz) || !grid.isWalkable(cx, cz + dzs[d])) return false;
        }
        return true;
    };

    using Node = std::pair<float, int>; // (cost, cell index)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    field.cost[idx(gx, gz)] = 0.0f;
    open.push({0.0f, static_cast<int>(idx(gx, gz))});

    while (!open.empty()) {
        const float c = open.top().first;
        const int cur = open.top().second;
        open.pop();
        if (c > field.cost[static_cast<std::size_t>(cur)]) continue; // stale entry
        const int cx = cur % w, cz = cur / w;
        for (int d = 0; d < 8; ++d) {
            if (!canStep(cx, cz, d)) continue;
            const int nx = cx + dxs[d], nz = cz + dzs[d];
            const float step = (dxs[d] != 0 && dzs[d] != 0) ? kSqrt2 : 1.0f;
            const float nc = c + step;
            if (nc < field.cost[idx(nx, nz)]) {
                field.cost[idx(nx, nz)] = nc;
                open.push({nc, static_cast<int>(idx(nx, nz))});
            }
        }
    }

    // Gradient: each reachable non-goal cell flows toward its lowest-cost
    // neighbour (steepest descent). That neighbour is always strictly downhill,
    // so following the field is guaranteed to reach the goal.
    for (int cz = 0; cz < h; ++cz) {
        for (int cx = 0; cx < w; ++cx) {
            const std::size_t i = idx(cx, cz);
            if (field.cost[i] == kInf) continue;
            if (cx == gx && cz == gz) continue; // goal cell: flow stays zero
            float best = field.cost[i];
            int bestDx = 0, bestDz = 0;
            for (int d = 0; d < 8; ++d) {
                if (!canStep(cx, cz, d)) continue;
                const int nx = cx + dxs[d], nz = cz + dzs[d];
                const float nCost = field.cost[idx(nx, nz)];
                if (nCost < best) {
                    best = nCost;
                    bestDx = dxs[d];
                    bestDz = dzs[d];
                }
            }
            if (bestDx != 0 || bestDz != 0) {
                const float inv = 1.0f / std::sqrt(static_cast<float>(bestDx * bestDx + bestDz * bestDz));
                field.flowX[i] = static_cast<float>(bestDx) * inv;
                field.flowZ[i] = static_cast<float>(bestDz) * inv;
            }
        }
    }
    return field;
}

/// Spawn a flow-field agent at @p pos and return its entity.
inline ecs::Entity spawnAgent(ecs::Registry& registry, const ecs::Vec3& pos, float speed = 4.0f,
                              float arriveRadius = 0.3f) {
    const ecs::Entity e = registry.create();
    ecs::Transform t;
    t.position = pos;
    registry.add<ecs::Transform>(e, t);
    registry.add<CrowdAgent>(e, CrowdAgent{speed, arriveRadius, false});
    return e;
}

/**
 * @brief Advance every CrowdAgent one fixed step of @p dt seconds along @p field.
 *
 * Iterates the (Transform, CrowdAgent) archetype columns directly via the ECS
 * query - the cache-friendly hot path that makes crowd scale practical. Each
 * agent steps along the flow direction, sliding along a blocked axis rather than
 * entering an obstacle cell, and snaps to the goal within its arrive radius.
 *
 * @return the number of agents still moving (not yet arrived); 0 means the whole
 *         crowd has reached the goal.
 */
inline int steerCrowd(ecs::Registry& registry, const world::NavGrid& grid, const FlowField& field,
                      float dt) {
    const float goalX = field.goal.x, goalZ = field.goal.z;
    const int goalCx = grid.cellX(goalX), goalCz = grid.cellZ(goalZ);
    int moving = 0;

    registry.view<ecs::Transform, CrowdAgent>().each(
        [&](ecs::Entity, ecs::Transform& t, CrowdAgent& a) {
            if (a.arrived) return;

            const float dxg = goalX - t.position.x, dzg = goalZ - t.position.z;
            const float distToGoal = std::sqrt(dxg * dxg + dzg * dzg);
            if (distToGoal <= a.arriveRadius) {
                t.position.x = goalX;
                t.position.z = goalZ;
                a.arrived = true;
                return;
            }

            // In the goal cell, steer straight at the goal point; otherwise follow
            // the precomputed flow. If somehow off-field, fall back to the goal
            // direction so the agent keeps trying rather than stalling.
            const int cx = grid.cellX(t.position.x), cz = grid.cellZ(t.position.z);
            ecs::Vec3 dir;
            if (cx == goalCx && cz == goalCz) {
                const float inv = distToGoal > 0.0f ? 1.0f / distToGoal : 0.0f;
                dir = ecs::Vec3{dxg * inv, 0.0f, dzg * inv};
            } else {
                dir = field.directionAt(t.position.x, t.position.z);
                if (dir.x == 0.0f && dir.z == 0.0f) {
                    const float inv = distToGoal > 0.0f ? 1.0f / distToGoal : 0.0f;
                    dir = ecs::Vec3{dxg * inv, 0.0f, dzg * inv};
                }
            }

            float stepLen = a.speed * dt;
            if (stepLen > distToGoal) stepLen = distToGoal; // do not overshoot the goal
            const float nx = t.position.x + dir.x * stepLen;
            const float nz = t.position.z + dir.z * stepLen;

            // Keep agents on walkable cells (static obstacle avoidance). If the
            // diagonal move is blocked, slide along whichever axis stays walkable.
            if (grid.isWalkable(grid.cellX(nx), grid.cellZ(nz))) {
                t.position.x = nx;
                t.position.z = nz;
            } else if (grid.isWalkable(grid.cellX(nx), grid.cellZ(t.position.z))) {
                t.position.x = nx;
            } else if (grid.isWalkable(grid.cellX(t.position.x), grid.cellZ(nz))) {
                t.position.z = nz;
            }
            // else: boxed in this step; hold and retry next step (no stall - the
            // flow still points downhill, this is at most a one-step pause).
            ++moving;
        });

    return moving;
}

} // namespace sim
} // namespace IKore
