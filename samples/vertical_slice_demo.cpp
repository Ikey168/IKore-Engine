// Sample: the roadmap vertical slice, end to end and headless.
//
// Imports a small neighborhood from GeoJSON, bakes a nav grid, spawns a crowd that
// navigates to a goal, then rewinds time and replays - printing what happens at
// each stage. No GL context required, so it runs anywhere.
//
// Build (also wired into CMake as sample_vertical_slice):
//   g++ -std=c++17 -I src samples/vertical_slice_demo.cpp -o sample_vertical_slice
//   ./sample_vertical_slice

#include "game/VerticalSlice.h"

#include <cstdio>

using namespace IKore;

// A tiny OSM-style neighborhood: two roads, two buildings, and a park.
static const char* kNeighborhood = R"JSON({
  "type": "FeatureCollection",
  "features": [
    { "type":"Feature", "properties":{"highway":"residential"},
      "geometry":{"type":"LineString","coordinates":[[0,0],[0.0001,0],[0.0002,0]]} },
    { "type":"Feature", "properties":{"highway":"residential"},
      "geometry":{"type":"LineString","coordinates":[[0.0002,0],[0.0002,0.0001]]} },
    { "type":"Feature", "properties":{"building":"yes","height":"10"},
      "geometry":{"type":"Polygon","coordinates":[[[0,0],[0.0001,0],[0.0001,0.0001],[0,0.0001],[0,0]]]} },
    { "type":"Feature", "properties":{"building":"yes","building:levels":"3"},
      "geometry":{"type":"Polygon","coordinates":[[[0.0003,0],[0.0004,0],[0.0004,0.0001],[0.0003,0.0001],[0.0003,0]]]} },
    { "type":"Feature", "properties":{"leisure":"park"},
      "geometry":{"type":"Polygon","coordinates":[[[0,0.0002],[0.0002,0.0002],[0.0002,0.0003],[0,0.0003],[0,0.0002]]]} }
  ]
})JSON";

// Pick a walkable cell in the far corner of the grid to use as the crowd's goal.
static ecs::Vec3 farWalkableCell(const world::NavGrid& grid) {
    ecs::Vec3 best{};
    float bestScore = -1.0e30f;
    for (int cz = 0; cz < grid.height; ++cz) {
        for (int cx = 0; cx < grid.width; ++cx) {
            if (!grid.isWalkable(cx, cz)) continue;
            const ecs::Vec3 c = grid.cellCenter(cx, cz);
            const float score = c.x + c.z;
            if (score > bestScore) {
                bestScore = score;
                best = c;
            }
        }
    }
    return best;
}

int main() {
    game::VerticalSlice::Config config;
    config.cellSize = 2.0f;
    config.historyCapacity = 3000; // retain the whole run so we can rewind to the midpoint
    game::VerticalSlice slice(config);

    if (!slice.loadFromGeoJson(kNeighborhood)) {
        std::printf("Failed to import neighborhood (no walkable space).\n");
        return 1;
    }
    std::printf("Imported neighborhood: %zu buildings, %zu roads, %zu regions.\n",
                slice.city().buildings.size(), slice.city().roads.size(),
                slice.city().regions.size());
    std::printf("Baked nav grid: %d x %d cells, %zu walkable.\n", slice.grid().width,
                slice.grid().height, slice.grid().walkableCount());

    const ecs::Vec3 goal = farWalkableCell(slice.grid());
    slice.setGoal(goal);
    const std::size_t spawned = slice.spawnCrowd(200);
    std::printf("Goal at (%.1f, %.1f); spawned %zu agents.\n", goal.x, goal.z, spawned);

    // Run the fixed-step simulation for a bounded number of ticks (agents that
    // land in a disconnected pocket may never reach the goal, so we cap it).
    int moving = slice.agentsMoving();
    for (int i = 0; i < 2000 && (i == 0 || moving > 0); ++i) moving = slice.step();
    const std::uint64_t endTick = slice.tick();
    const std::size_t arrivedAtEnd = slice.arrivedCount();
    std::printf("After %llu ticks: %zu of %zu agents reached the goal.\n",
                static_cast<unsigned long long>(endTick), arrivedAtEnd, slice.agentCount());

    // Rewind to the halfway point, then replay forward - the crowd is restored and
    // the replay reproduces the same outcome (deterministic time control).
    const std::uint64_t midpoint = endTick / 2;
    if (slice.rewindTo(midpoint)) {
        std::printf("Rewound to tick %llu: %zu agents had arrived by then.\n",
                    static_cast<unsigned long long>(midpoint), slice.arrivedCount());
        while (slice.tick() < endTick) slice.step();
        std::printf("Replayed forward to tick %llu: %zu agents arrived (matches the first run: %s).\n",
                    static_cast<unsigned long long>(slice.tick()), slice.arrivedCount(),
                    slice.arrivedCount() == arrivedAtEnd ? "yes" : "no");
    }

    std::printf("Done.\n");
    return 0;
}
