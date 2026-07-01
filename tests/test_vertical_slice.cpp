// Test for the vertical-slice integration - roadmap EXPANSION_IDEAS section 6.
//
// Ties the finished foundations together and verifies the whole path: import a
// neighborhood (GeoJSON), bake a nav grid, build a flow field, spawn a crowd that
// navigates to the goal, and rewind/replay the run deterministically.
//
// Pure std + header-only (importer, nav mesh, crowd, timeline, ECS are all
// header-only and glm-free):
//   g++ -std=c++17 -I src tests/test_vertical_slice.cpp -o test_vertical_slice

#include "game/VerticalSlice.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
namespace game = IKore::game;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// A real-ish OSM-style neighborhood: two roads sharing a node, two buildings, and
// a park - the same shape the GeoJSON importer test uses.
static const char* kGeoJson = R"JSON({
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

// A hand-built city: a large open plaza (defines the extent) with one building
// obstacle in the middle. Deterministic and importer-independent.
static world::City makePlazaCity() {
    world::City city;

    world::Region plaza;
    plaza.footprint = {{0.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 40.0f}};
    city.regions.push_back(plaza);

    world::Building block;
    block.center = {20.0f, 3.0f, 20.0f};
    block.size = {6.0f, 6.0f, 6.0f};
    block.footprint = {{17.0f, 0.0f, 17.0f}, {23.0f, 0.0f, 17.0f}, {23.0f, 0.0f, 23.0f}, {17.0f, 0.0f, 23.0f}};
    city.buildings.push_back(block);

    return city;
}

static std::vector<ecs::Vec3> snapshotPositions(const game::VerticalSlice& slice) {
    std::vector<ecs::Vec3> out;
    for (std::size_t i = 0; i < slice.agentCount(); ++i) out.push_back(slice.agentPosition(i));
    return out;
}

static bool samePositions(const std::vector<ecs::Vec3>& a, const std::vector<ecs::Vec3>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) return false;
    }
    return true;
}

static void stepTo(game::VerticalSlice& slice, std::uint64_t target) {
    while (slice.tick() < target) slice.step();
}

static void testImportAndBake() {
    game::VerticalSlice slice({/*cellSize=*/2.0f});
    CHECK(slice.loadFromGeoJson(kGeoJson));
    CHECK(slice.city().buildings.size() == 2);
    CHECK(slice.city().roads.size() == 2);
    CHECK(slice.grid().walkableCount() > 0); // buildings block some cells, not all
}

static void testSpawnOnWalkableCells() {
    game::VerticalSlice slice;
    CHECK(slice.buildFrom(makePlazaCity()));
    slice.setGoal(ecs::Vec3{36.0f, 0.0f, 36.0f});
    CHECK(slice.hasGoal());

    const std::size_t spawned = slice.spawnCrowd(32);
    CHECK(spawned == 32);
    CHECK(slice.agentCount() == 32);
    CHECK(slice.arrivedCount() == 0); // none have moved yet

    // Every agent starts on a walkable cell.
    for (std::size_t i = 0; i < slice.agentCount(); ++i) {
        const ecs::Vec3 p = slice.agentPosition(i);
        CHECK(slice.grid().isWalkable(slice.grid().cellX(p.x), slice.grid().cellZ(p.z)));
    }
}

static void testCrowdReachesGoal() {
    game::VerticalSlice slice({/*cellSize=*/1.0f, /*agentRadius=*/0.3f, /*boundsMargin=*/4.0f,
                               /*agentSpeed=*/8.0f});
    CHECK(slice.buildFrom(makePlazaCity()));
    slice.setGoal(ecs::Vec3{36.0f, 0.0f, 36.0f});
    slice.spawnCrowd(16);
    CHECK(slice.agentCount() == 16);

    // Drive the simulation until the crowd arrives (bounded so the test terminates).
    int moving = slice.agentsMoving();
    for (int i = 0; i < 4000 && (i == 0 || moving > 0); ++i) moving = slice.step();

    CHECK(slice.agentsMoving() == 0);
    CHECK(slice.arrivedCount() == slice.agentCount()); // all reached the goal
}

static void testDeterministicRewindAndReplay() {
    game::VerticalSlice slice({/*cellSize=*/1.0f, /*agentRadius=*/0.3f, /*boundsMargin=*/4.0f,
                               /*agentSpeed=*/6.0f});
    slice.buildFrom(makePlazaCity());
    slice.setGoal(ecs::Vec3{36.0f, 0.0f, 36.0f});
    slice.spawnCrowd(24);

    stepTo(slice, 30);
    CHECK(slice.tick() == 30);
    const std::vector<ecs::Vec3> at30 = snapshotPositions(slice);

    stepTo(slice, 60);
    CHECK(slice.tick() == 60);
    const std::vector<ecs::Vec3> at60 = snapshotPositions(slice);

    // The two snapshots differ (the crowd actually moved between them).
    CHECK(!samePositions(at30, at60));

    // Rewind restores the exact state at tick 30.
    CHECK(slice.rewindTo(30));
    CHECK(slice.tick() == 30);
    CHECK(samePositions(snapshotPositions(slice), at30));

    // Replaying forward reproduces tick 60 bit-for-bit (deterministic).
    stepTo(slice, 60);
    CHECK(samePositions(snapshotPositions(slice), at60));

    // Rewinding outside the retained window fails without changing state.
    const std::uint64_t current = slice.tick();
    CHECK(!slice.rewindTo(current + 100));
    CHECK(slice.tick() == current);
}

int main() {
    testImportAndBake();
    testSpawnOnWalkableCells();
    testCrowdReachesGoal();
    testDeterministicRewindAndReplay();

    if (g_failures == 0) {
        std::printf("All vertical slice tests passed.\n");
        return 0;
    }
    std::printf("%d vertical slice test(s) failed.\n", g_failures);
    return 1;
}
