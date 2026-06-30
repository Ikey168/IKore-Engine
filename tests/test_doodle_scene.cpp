// Test for the renderer-agnostic drawing -> scene converter - M15, #162.
//
// Verifies the issue's acceptance criteria:
//   - The converter has no renderer dependencies and is unit-testable headless:
//     this test builds and runs with only the header-only ECS / world / game
//     headers (no glm, no OpenGL).
//   - Wall polylines extrude to 3D geometry: a room outline becomes oriented wall
//     boxes and a triangle mesh that rises from the floor (y=0) to wallHeight.
//   - Symbols become entity spawns: placed symbols turn into EntitySpawns and,
//     when emitted, into ECS entities carrying their type.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_doodle_scene.cpp -o test_doodle_scene

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"
#include "core/ecs/components/Components.h"
#include "game/DoodleScene.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

// A square room (closed polyline, 4 segments) plus three symbols.
static LevelSpec sampleLevel() {
    LevelSpec spec;
    spec.wallHeight = 3.0f;
    spec.wallThickness = 0.2f;
    Wall room;
    room.polyline = {{0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10}, {0, 0, 0}};
    spec.walls.push_back(room);
    spec.symbols = {
        {"player", {5, 0, 5}, 0.0f},
        {"enemy", {8, 0, 2}, 0.0f},
        {"treasure", {2, 0, 8}, 0.0f},
    };
    return spec;
}

static void testWallsExtrudeTo3DGeometry() {
    const SceneDescription scene = convert(sampleLevel());

    // Four segments -> four oriented wall boxes.
    CHECK(scene.wallBoxes.size() == 4);
    // First segment (0,0)->(10,0): centered at (5,1.5,0), length 10, flat (yaw 0).
    const world::Box& b0 = scene.wallBoxes[0];
    CHECK(approx(b0.center.x, 5.0f) && approx(b0.center.z, 0.0f));
    CHECK(approx(b0.center.y, 1.5f));           // half of wallHeight
    CHECK(approx(b0.size.x, 10.0f));            // segment length
    CHECK(approx(b0.size.y, 3.0f));             // wallHeight
    CHECK(approx(b0.size.z, 0.2f));             // wallThickness
    CHECK(approx(b0.yaw, 0.0f));
    // Second segment (10,0)->(10,10): turns 90 degrees.
    CHECK(approx(scene.wallBoxes[1].yaw, 3.14159265f / 2.0f, 1e-3f));

    // The extruded mesh: 24 verts + 36 indices per box, all indices in range.
    CHECK(scene.wallMesh.vertices.size() == 24u * 4u);
    CHECK(scene.wallMesh.indices.size() == 36u * 4u);
    bool indicesInRange = true;
    for (std::uint32_t idx : scene.wallMesh.indices) {
        if (idx >= scene.wallMesh.vertices.size()) indicesInRange = false;
    }
    CHECK(indicesInRange);

    // Geometry rises from the floor (y=0) to wallHeight, and normals are unit.
    float minY = 1e9f, maxY = -1e9f;
    bool normalsUnit = true;
    for (const MeshVertex& v : scene.wallMesh.vertices) {
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
        if (!approx(v.normal.length(), 1.0f, 1e-3f)) normalsUnit = false;
    }
    CHECK(approx(minY, 0.0f));
    CHECK(approx(maxY, 3.0f));
    CHECK(normalsUnit);
}

static void testSymbolsBecomeEntitySpawns() {
    const SceneDescription scene = convert(sampleLevel());

    CHECK(scene.spawns.size() == 3);
    // Spawns preserve type and position.
    bool foundPlayer = false;
    for (const EntitySpawn& s : scene.spawns) {
        if (s.type == "player") {
            foundPlayer = true;
            CHECK(approx(s.position.x, 5.0f) && approx(s.position.z, 5.0f));
        }
    }
    CHECK(foundPlayer);
}

static void testEmitToRegistry() {
    const SceneDescription scene = convert(sampleLevel());
    ecs::Registry reg;
    const std::size_t created = emitToRegistry(scene, reg);

    // 4 wall entities + 3 spawn entities.
    CHECK(created == 7);
    CHECK(reg.aliveCount() == 7);
    CHECK(reg.view<ecs::Transform>().count() == 7);
    // Only the spawns carry a SpawnTag.
    CHECK((reg.view<ecs::Transform, SpawnTag>().count() == 3));

    // The spawn tags carry the symbol types.
    int taggedPlayers = 0;
    reg.view<ecs::Transform, SpawnTag>().each([&](ecs::Entity, ecs::Transform&, SpawnTag& tag) {
        if (tag.type == "player") ++taggedPlayers;
    });
    CHECK(taggedPlayers == 1);
}

static void testEmptyAndDegenerate() {
    // An empty spec yields an empty scene.
    const SceneDescription empty = convert(LevelSpec{});
    CHECK(empty.wallBoxes.empty());
    CHECK(empty.wallMesh.vertices.empty());
    CHECK(empty.spawns.empty());

    // A zero-length segment is skipped (no degenerate box).
    LevelSpec spec;
    Wall w;
    w.polyline = {{1, 0, 1}, {1, 0, 1}, {4, 0, 1}}; // first segment degenerate
    spec.walls.push_back(w);
    const SceneDescription scene = convert(spec);
    CHECK(scene.wallBoxes.size() == 1); // only the real segment survives
}

int main() {
    testWallsExtrudeTo3DGeometry();
    testSymbolsBecomeEntitySpawns();
    testEmitToRegistry();
    testEmptyAndDegenerate();

    if (g_failures == 0) {
        std::printf("All doodle scene converter tests passed.\n");
        return 0;
    }
    std::printf("%d doodle scene test(s) failed.\n", g_failures);
    return 1;
}
