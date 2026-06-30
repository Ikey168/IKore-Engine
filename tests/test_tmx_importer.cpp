// Test for the Tiled (.tmx) tilemap importer - Milestone 11, issue #149.
//
// Verifies CSV tile layers map to a grid of 3D cells (empty GID 0 skipped, flip
// flags stripped), object layers spawn entities at the right positions, and the
// result emits into the ECS. Pure std + the header-only ECS:
//   g++ -std=c++17 -I src tests/test_tmx_importer.cpp -o test_tmx_importer

#include "core/ecs/View.h"
#include "world/TmxImporter.h"

#include <cmath>
#include <cstdio>
#include <string>

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

// 3x2 orthogonal map, 32px tiles. Tile layer (CSV, row-major):
//   row0: 1, 0, 2     row1: 0, 3, 0
// Object group: a point spawn ("player") and a tile object ("chest", gid 5).
static const char* kTmx = R"TMX(<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" orientation="orthogonal" width="3" height="2" tilewidth="32" tileheight="32">
 <layer id="1" name="ground" width="3" height="2">
  <data encoding="csv">
1,0,2,
0,3,0
</data>
 </layer>
 <objectgroup id="2" name="spawns">
  <object id="1" name="player" type="spawn" x="64" y="32" width="0" height="0"/>
  <object id="2" name="chest" type="prop" gid="5" x="0" y="0" width="32" height="32"/>
 </objectgroup>
</map>)TMX";

int main() {
    const world::TileLevel level = world::importString(kTmx); // defaults: scale 1, tileHeight 1

    CHECK(level.width == 3);
    CHECK(level.height == 2);
    CHECK(level.tileWidth == 32);
    CHECK(level.tileHeight == 32);

    // Three non-empty tiles, in row-major order.
    CHECK(level.tiles.size() == 3);

    CHECK(level.tiles[0].gid == 1);
    CHECK(level.tiles[0].col == 0 && level.tiles[0].row == 0);
    CHECK(approx(level.tiles[0].center.x, 16.0f));   // (0.5)*32
    CHECK(approx(level.tiles[0].center.y, 0.5f));    // tileWorldHeight/2
    CHECK(approx(level.tiles[0].center.z, 16.0f));
    CHECK(approx(level.tiles[0].size.x, 32.0f));
    CHECK(approx(level.tiles[0].size.z, 32.0f));

    CHECK(level.tiles[1].gid == 2);
    CHECK(level.tiles[1].col == 2 && level.tiles[1].row == 0);
    CHECK(approx(level.tiles[1].center.x, 80.0f));   // (2.5)*32

    CHECK(level.tiles[2].gid == 3);
    CHECK(level.tiles[2].col == 1 && level.tiles[2].row == 1);
    CHECK(approx(level.tiles[2].center.x, 48.0f));   // (1.5)*32
    CHECK(approx(level.tiles[2].center.z, 48.0f));

    // Two objects spawned at the right positions.
    CHECK(level.objects.size() == 2);

    CHECK(level.objects[0].name == "player");
    CHECK(level.objects[0].type == "spawn");
    CHECK(level.objects[0].gid == 0);
    CHECK(approx(level.objects[0].center.x, 64.0f)); // point object at (64,32)
    CHECK(approx(level.objects[0].center.z, 32.0f));

    CHECK(level.objects[1].name == "chest");
    CHECK(level.objects[1].gid == 5);
    CHECK(approx(level.objects[1].center.x, 16.0f)); // (0 + 32/2)
    CHECK(approx(level.objects[1].center.z, 16.0f));
    CHECK(approx(level.objects[1].size.x, 32.0f));

    // Emit into the ECS: one entity per tile + per object.
    ecs::Registry world;
    const std::size_t created = world::emitToRegistry(level, world);
    CHECK(created == 5); // 3 tiles + 2 objects
    CHECK(world.view<ecs::Transform>().count() == 5);

    if (g_failures == 0) {
        std::printf("All TMX importer tests passed.\n");
        return 0;
    }
    std::printf("%d TMX importer test(s) failed.\n", g_failures);
    return 1;
}
