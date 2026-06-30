// Test for navigation baking + pathfinding - Milestone 11, issue #151.
//
// Verifies a NavGrid bakes from obstacle boxes and A* finds a path between two
// reachable points, routes through a gap (door) in a dividing wall, and reports
// no path when the regions are fully separated. Pure std + header-only ECS types:
//   g++ -std=c++17 -I src tests/test_navmesh.cpp -o test_navmesh

#include "world/NavMesh.h"

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

static bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

static bool allWalkable(const world::NavGrid& grid, const std::vector<ecs::Vec3>& path) {
    for (const ecs::Vec3& p : path) {
        if (!grid.isWalkable(grid.cellX(p.x), grid.cellZ(p.z))) return false;
    }
    return true;
}

int main() {
    const ecs::Vec3 start{0.5f, 0.0f, 0.5f}; // cell (0,0)
    const ecs::Vec3 goal{9.5f, 0.0f, 0.5f};  // cell (9,0)

    // --- Open field: a direct path exists. ---
    {
        const world::NavGrid grid = world::bake(0, 0, 10, 10, 1.0f, {});
        CHECK(grid.width == 10 && grid.height == 10);
        CHECK(grid.walkableCount() == 100);
        const auto path = world::findPath(grid, start, goal);
        CHECK(!path.empty());
        CHECK(approx(path.front().x, 0.5f) && approx(path.front().z, 0.5f));
        CHECK(approx(path.back().x, 9.5f) && approx(path.back().z, 0.5f));
        CHECK(allWalkable(grid, path));
    }

    // --- Dividing wall at x=5 with a gap (door) at z in [3,7]. ---
    {
        std::vector<world::Obstacle> obstacles = {
            {{5.0f, 0.0f, 1.5f}, {0.5f, 3.0f, 3.0f}, 0.0f}, // covers z ~0..3
            {{5.0f, 0.0f, 8.5f}, {0.5f, 3.0f, 3.0f}, 0.0f}, // covers z ~7..10
        };
        const world::NavGrid grid = world::bake(0, 0, 10, 10, 1.0f, obstacles);
        CHECK(grid.walkableCount() > 0 && grid.walkableCount() < 100); // some blocked

        const auto path = world::findPath(grid, start, goal);
        CHECK(!path.empty()); // reachable through the gap
        CHECK(approx(path.front().x, 0.5f));
        CHECK(approx(path.back().x, 9.5f));
        CHECK(allWalkable(grid, path));

        // It had to detour up to the gap (rows ~4-6) to cross x=5.
        float maxZ = 0.0f;
        for (const ecs::Vec3& p : path) maxZ = std::max(maxZ, p.z);
        CHECK(maxZ >= 4.0f);
    }

    // --- Full wall at x=5, no gap: the two sides are disconnected. ---
    {
        std::vector<world::Obstacle> obstacles = {
            {{5.0f, 0.0f, 5.0f}, {0.5f, 3.0f, 10.0f}, 0.0f}, // spans the whole z range
        };
        const world::NavGrid grid = world::bake(0, 0, 10, 10, 1.0f, obstacles);
        const auto path = world::findPath(grid, start, goal);
        CHECK(path.empty()); // no route across a solid wall
    }

    if (g_failures == 0) {
        std::printf("All nav-mesh tests passed.\n");
        return 0;
    }
    std::printf("%d nav-mesh test(s) failed.\n", g_failures);
    return 1;
}
