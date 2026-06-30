// Test for the level-spec + 3D-scene JSON formats - Milestone 15, #163.
//
// Verifies the issue's acceptance criteria:
//   - A documented schema round-trips through save/load: a LevelSpec and a
//     converted SceneDescription each survive toJson -> fromJson unchanged.
//   - A hand-authored sample level loads into a playable scene: the sample JSON
//     (matching assets/levels/sample_dungeon.level.json) parses, converts, and
//     emits walls plus a player and other entity spawns into an ECS registry.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_level_format.cpp -o test_level_format

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"
#include "core/ecs/components/Components.h"
#include "game/DoodleScene.h"
#include "game/LevelFormat.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace IKore;
using namespace IKore::game;

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

// Matches assets/levels/sample_dungeon.level.json (kept in sync by hand).
static const char* kSampleLevel = R"({
  "format": "doodle-level",
  "version": 1,
  "wallHeight": 3.0,
  "wallThickness": 0.2,
  "walls": [
    {"polyline": [[0, 0], [20, 0], [20, 15], [0, 15], [0, 0]]},
    {"polyline": [[8, 0], [8, 9]]}
  ],
  "symbols": [
    {"type": "player", "x": 2, "z": 2, "yaw": 0},
    {"type": "enemy", "x": 16, "z": 4, "yaw": 0},
    {"type": "enemy", "x": 12, "z": 12, "yaw": 0},
    {"type": "treasure", "x": 18, "z": 13, "yaw": 0},
    {"type": "door", "x": 8, "z": 11, "yaw": 0}
  ]
})";

static LevelSpec sampleSpec() {
    LevelSpec s;
    s.wallHeight = 3.0f;
    s.wallThickness = 0.2f;
    Wall outer;
    outer.polyline = {{0, 0, 0}, {20, 0, 0}, {20, 0, 15}, {0, 0, 15}, {0, 0, 0}};
    Wall inner;
    inner.polyline = {{8, 0, 0}, {8, 0, 9}};
    s.walls = {outer, inner};
    s.symbols = {
        {"player", {2, 0, 2}, 0.0f},
        {"enemy", {16, 0, 4}, 0.0f},
        {"treasure", {18, 0, 13}, 0.0f},
    };
    return s;
}

static void testLevelSpecRoundTrips() {
    const LevelSpec in = sampleSpec();
    LevelSpec out;
    CHECK(fromLevelJson(toLevelJson(in), out));

    CHECK(approx(out.wallHeight, in.wallHeight));
    CHECK(approx(out.wallThickness, in.wallThickness));
    CHECK(out.walls.size() == in.walls.size());
    for (std::size_t w = 0; w < in.walls.size(); ++w) {
        CHECK(out.walls[w].polyline.size() == in.walls[w].polyline.size());
        for (std::size_t p = 0; p < in.walls[w].polyline.size(); ++p) {
            CHECK(approx(out.walls[w].polyline[p].x, in.walls[w].polyline[p].x));
            CHECK(approx(out.walls[w].polyline[p].z, in.walls[w].polyline[p].z));
        }
    }
    CHECK(out.symbols.size() == in.symbols.size());
    for (std::size_t i = 0; i < in.symbols.size(); ++i) {
        CHECK(out.symbols[i].type == in.symbols[i].type);
        CHECK(approx(out.symbols[i].position.x, in.symbols[i].position.x));
        CHECK(approx(out.symbols[i].position.z, in.symbols[i].position.z));
    }
}

static void testSceneRoundTrips() {
    const SceneDescription scene = convert(sampleSpec());
    SceneDescription out;
    CHECK(fromSceneJson(toSceneJson(scene), out));

    CHECK(out.wallBoxes.size() == scene.wallBoxes.size());
    for (std::size_t i = 0; i < scene.wallBoxes.size(); ++i) {
        CHECK(approx(out.wallBoxes[i].center.x, scene.wallBoxes[i].center.x));
        CHECK(approx(out.wallBoxes[i].center.y, scene.wallBoxes[i].center.y));
        CHECK(approx(out.wallBoxes[i].center.z, scene.wallBoxes[i].center.z));
        CHECK(approx(out.wallBoxes[i].size.x, scene.wallBoxes[i].size.x));
        CHECK(approx(out.wallBoxes[i].yaw, scene.wallBoxes[i].yaw));
    }
    CHECK(out.spawns.size() == scene.spawns.size());
    for (std::size_t i = 0; i < scene.spawns.size(); ++i) {
        CHECK(out.spawns[i].type == scene.spawns[i].type);
        CHECK(approx(out.spawns[i].position.x, scene.spawns[i].position.x));
    }
}

static void testSampleLevelLoadsToPlayableScene() {
    LevelSpec spec;
    CHECK(fromLevelJson(kSampleLevel, spec));
    CHECK(spec.walls.size() == 2);   // outer room + inner wall
    CHECK(spec.symbols.size() == 5); // player, 2 enemies, treasure, door
    CHECK(approx(spec.wallHeight, 3.0f));

    const SceneDescription scene = convert(spec);
    CHECK(scene.wallBoxes.size() == 5);          // 4 outer segments + 1 inner
    CHECK(!scene.wallMesh.vertices.empty());     // extruded geometry exists
    CHECK(scene.spawns.size() == 5);

    ecs::Registry reg;
    const std::size_t created = emitToRegistry(scene, reg);
    CHECK(created == 10);                         // 5 walls + 5 spawns
    CHECK(reg.aliveCount() == 10);

    // A playable scene: walls, a player start, and other tagged entities.
    int players = 0, total = 0;
    reg.view<ecs::Transform, SpawnTag>().each([&](ecs::Entity, ecs::Transform&, SpawnTag& tag) {
        ++total;
        if (tag.type == "player") ++players;
    });
    CHECK(total == 5);
    CHECK(players == 1);
}

static void testMalformedReturnsFalse() {
    LevelSpec spec;
    CHECK(!fromLevelJson("{ this is not json", spec));
    CHECK(!fromLevelJson("[1, 2, 3]", spec)); // valid JSON but not an object
    CHECK(!fromLevelJson("", spec));
}

int main() {
    testLevelSpecRoundTrips();
    testSceneRoundTrips();
    testSampleLevelLoadsToPlayableScene();
    testMalformedReturnsFalse();

    if (g_failures == 0) {
        std::printf("All level-format tests passed.\n");
        return 0;
    }
    std::printf("%d level-format test(s) failed.\n", g_failures);
    return 1;
}
