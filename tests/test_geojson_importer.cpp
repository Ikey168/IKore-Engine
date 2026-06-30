// Test for the GeoJSON/OSM importer - Milestone 11, issue #150.
//
// Verifies buildings extrude from footprints (height + levels tags), roads carry
// a width from the highway type and are smoothed, roads sharing a node produce an
// intersection, water/park polygons become regions, and it all emits into the
// ECS. Pure std + the header-only ECS:
//   g++ -std=c++17 -I src tests/test_geojson_importer.cpp -o test_geojson_importer

#include "core/ecs/View.h"
#include "world/GeoJsonImporter.h"

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

static bool approx(float a, float b, float eps = 0.05f) {
    return std::fabs(a - b) <= eps;
}

// Origin is the first coordinate of the first feature: (0,0). At lat 0,
// 1 degree lon/lat = 111320 m, so 0.0001 deg = 11.132 m, 0.0002 deg = 22.264 m.
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

int main() {
    const world::City city = world::importString(kGeoJson); // defaults

    // Counts.
    CHECK(city.buildings.size() == 2);
    CHECK(city.roads.size() == 2);
    CHECK(city.regions.size() == 1);

    // Building heights: explicit height tag, and levels x floorHeight (3 * 3).
    CHECK(approx(city.buildings[0].height, 10.0f));
    CHECK(approx(city.buildings[1].height, 9.0f));
    CHECK(approx(city.buildings[0].center.y, 5.0f)); // height/2
    CHECK(approx(city.buildings[0].size.y, 10.0f));
    // Footprint 0.0001 deg square -> ~11.132 m on a side.
    CHECK(approx(city.buildings[0].size.x, 11.132f));
    CHECK(approx(city.buildings[0].size.z, 11.132f));

    // Road width from highway type, and corner smoothing added points.
    CHECK(approx(city.roads[0].width, 5.0f));
    CHECK(city.roads[0].centerline.size() == 6); // 3 pts -> Chaikin -> 6
    CHECK(approx(city.roads[0].centerline.front().x, 0.0f));
    CHECK(approx(city.roads[0].centerline.front().z, 0.0f));
    CHECK(approx(city.roads[0].centerline.back().x, 22.264f)); // 0.0002 * 111320
    CHECK(approx(city.roads[0].centerline.back().z, 0.0f));

    // The two roads share the node at (0.0002, 0) -> one intersection.
    CHECK(city.intersections.size() == 1);
    CHECK(approx(city.intersections[0].x, 22.264f));
    CHECK(approx(city.intersections[0].z, 0.0f));

    // Emit into the ECS: buildings + regions + one box per road segment.
    std::size_t expected = city.buildings.size() + city.regions.size();
    for (const world::Road& r : city.roads) expected += (r.centerline.size() - 1);

    ecs::Registry world;
    const std::size_t created = world::emitToRegistry(city, world);
    CHECK(created == expected);
    CHECK(world.view<ecs::Transform>().count() == expected);

    if (g_failures == 0) {
        std::printf("All GeoJSON importer tests passed.\n");
        return 0;
    }
    std::printf("%d GeoJSON importer test(s) failed.\n", g_failures);
    return 1;
}
