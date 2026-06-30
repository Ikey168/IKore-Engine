#pragma once

#include "core/ecs/components/Components.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

/**
 * @file NavMesh.h
 * @brief Navigation baking + pathfinding for imported worlds (issue #151).
 *
 * Milestone 11 capstone. Bakes a walkable navigation grid from the obstacle
 * geometry produced by the importers (#148-#150) - the wall and building boxes -
 * and finds paths across it with A*, so agents can navigate imported scenes.
 *
 * A grid (occupancy + A*) is a simple, robust navigation representation; it
 * avoids a heavy Recast/Detour dependency while satisfying the milestone (a
 * valid bake; an agent can path between two reachable points). Obstacles are
 * supplied explicitly (walls/buildings), so walkable surfaces like floors and
 * roads are not treated as blockers. Pure std + the header-only ECS types, so it
 * is unit-tested in isolation.
 */
namespace IKore {
namespace world {

/// An oriented obstacle box; only its XZ footprint matters for navigation.
struct Obstacle {
    ecs::Vec3 center{};
    ecs::Vec3 size{};
    float yaw{0.0f};
};

/// A baked walkable grid over the XZ plane.
class NavGrid {
public:
    float minX{0.0f}, minZ{0.0f}, cellSize{1.0f};
    int width{0}, height{0};
    std::vector<std::uint8_t> walkable; // row-major [z*width + x], 1 = walkable

    bool inBounds(int cx, int cz) const { return cx >= 0 && cz >= 0 && cx < width && cz < height; }
    bool isWalkable(int cx, int cz) const {
        return inBounds(cx, cz) && walkable[static_cast<std::size_t>(cz) * width + cx] != 0;
    }
    int cellX(float worldX) const { return static_cast<int>(std::floor((worldX - minX) / cellSize)); }
    int cellZ(float worldZ) const { return static_cast<int>(std::floor((worldZ - minZ) / cellSize)); }
    ecs::Vec3 cellCenter(int cx, int cz) const {
        return ecs::Vec3{minX + (static_cast<float>(cx) + 0.5f) * cellSize, 0.0f,
                         minZ + (static_cast<float>(cz) + 0.5f) * cellSize};
    }
    std::size_t walkableCount() const {
        std::size_t n = 0;
        for (std::uint8_t w : walkable) n += (w != 0);
        return n;
    }
};

/**
 * @brief Bake a NavGrid over [minX,maxX] x [minZ,maxZ]: cells overlapping an
 *        obstacle (inflated by @p agentRadius) are blocked, the rest walkable.
 */
inline NavGrid bake(float minX, float minZ, float maxX, float maxZ, float cellSize,
                    const std::vector<Obstacle>& obstacles, float agentRadius = 0.0f) {
    NavGrid grid;
    grid.minX = minX;
    grid.minZ = minZ;
    grid.cellSize = cellSize > 0.0f ? cellSize : 1.0f;
    grid.width = std::max(0, static_cast<int>(std::ceil((maxX - minX) / grid.cellSize)));
    grid.height = std::max(0, static_cast<int>(std::ceil((maxZ - minZ) / grid.cellSize)));
    grid.walkable.assign(static_cast<std::size_t>(grid.width) * grid.height, 1);

    for (const Obstacle& obs : obstacles) {
        const float hx = std::fabs(obs.size.x) * 0.5f;
        const float hz = std::fabs(obs.size.z) * 0.5f;
        const float c = std::fabs(std::cos(obs.yaw));
        const float s = std::fabs(std::sin(obs.yaw));
        // Axis-aligned bounds of the rotated footprint, inflated by the agent.
        const float ax = hx * c + hz * s + agentRadius;
        const float az = hx * s + hz * c + agentRadius;
        const float oMinX = obs.center.x - ax, oMaxX = obs.center.x + ax;
        const float oMinZ = obs.center.z - az, oMaxZ = obs.center.z + az;

        int cx0 = grid.cellX(oMinX), cx1 = grid.cellX(oMaxX);
        int cz0 = grid.cellZ(oMinZ), cz1 = grid.cellZ(oMaxZ);
        cx0 = std::max(cx0, 0);
        cz0 = std::max(cz0, 0);
        cx1 = std::min(cx1, grid.width - 1);
        cz1 = std::min(cz1, grid.height - 1);
        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                grid.walkable[static_cast<std::size_t>(cz) * grid.width + cx] = 0;
            }
        }
    }
    return grid;
}

/**
 * @brief A* path from @p start to @p goal across the grid (8-connected).
 * @return world-space waypoints (cell centers) from start cell to goal cell, or
 *         an empty vector if either endpoint is blocked/out of bounds or no path
 *         exists.
 */
inline std::vector<ecs::Vec3> findPath(const NavGrid& grid, const ecs::Vec3& start,
                                       const ecs::Vec3& goal) {
    std::vector<ecs::Vec3> path;
    const int sx = grid.cellX(start.x), sz = grid.cellZ(start.z);
    const int gx = grid.cellX(goal.x), gz = grid.cellZ(goal.z);
    if (!grid.isWalkable(sx, sz) || !grid.isWalkable(gx, gz)) return path;

    const int n = grid.width * grid.height;
    const auto idx = [&](int x, int z) { return static_cast<std::size_t>(z) * grid.width + x; };
    const float kInf = std::numeric_limits<float>::infinity();
    const float kSqrt2 = 1.41421356f;

    std::vector<float> g(static_cast<std::size_t>(n), kInf);
    std::vector<int> came(static_cast<std::size_t>(n), -1);
    auto heuristic = [&](int x, int z) {
        const float dx = static_cast<float>(std::abs(x - gx));
        const float dz = static_cast<float>(std::abs(z - gz));
        return (dx + dz) + (kSqrt2 - 2.0f) * std::min(dx, dz); // octile
    };

    using Node = std::pair<float, int>; // (f-score, cell index)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    g[idx(sx, sz)] = 0.0f;
    open.push({heuristic(sx, sz), static_cast<int>(idx(sx, sz))});

    const int dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dzs[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    bool found = false;
    while (!open.empty()) {
        const int cur = open.top().second;
        open.pop();
        const int cx = cur % grid.width;
        const int cz = cur / grid.width;
        if (cx == gx && cz == gz) {
            found = true;
            break;
        }
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + dxs[d], nz = cz + dzs[d];
            if (!grid.isWalkable(nx, nz)) continue;
            const bool diagonal = (dxs[d] != 0 && dzs[d] != 0);
            if (diagonal && (!grid.isWalkable(cx + dxs[d], cz) || !grid.isWalkable(cx, cz + dzs[d]))) {
                continue; // no cutting across obstacle corners
            }
            const float step = diagonal ? kSqrt2 : 1.0f;
            const float tentative = g[idx(cx, cz)] + step;
            if (tentative < g[idx(nx, nz)]) {
                g[idx(nx, nz)] = tentative;
                came[idx(nx, nz)] = cur;
                open.push({tentative + heuristic(nx, nz), static_cast<int>(idx(nx, nz))});
            }
        }
    }
    if (!found) return path;

    for (int cell = static_cast<int>(idx(gx, gz)); cell != -1; cell = came[static_cast<std::size_t>(cell)]) {
        path.push_back(grid.cellCenter(cell % grid.width, cell / grid.width));
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace world
} // namespace IKore
