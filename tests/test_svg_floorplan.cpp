// Test for the SVG floor-plan importer - Milestone 11, issue #148.
//
// Verifies the importer turns SVG floor-plan elements into oriented 3D wall
// boxes, recognizes door-layer elements as openings, generates a floor, and
// emits entities into the ECS. Pure std + the header-only ECS:
//   g++ -std=c++17 -I src tests/test_svg_floorplan.cpp -o test_svg_floorplan

#include "core/ecs/View.h"
#include "world/SvgFloorPlanImporter.h"

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

// 10 x 8 room (rect) + an interior line wall + a path wall + a polyline wall,
// plus a door opening on the bottom edge (class="door").
static const char* kSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg">
  <!-- room perimeter -->
  <rect x="0" y="0" width="10" height="8" />
  <line x1="0" y1="4" x2="6" y2="4" />
  <path d="M 6 4 L 6 8" />
  <polyline points="0,0 0,8" />
  <line class="door" x1="3" y1="8" x2="5" y2="8" />
</svg>)SVG";

int main() {
    world::ImportOptions opts; // defaults: scale 1, height 3, thickness 0.2, floor 0.1
    const world::FloorPlan plan = world::importString(kSvg, opts);

    // rect(4) + line(1) + path(1) + polyline(1) = 7 walls; 1 door.
    CHECK(plan.walls.size() == 7);
    CHECK(plan.doors.size() == 1);
    CHECK(plan.hasFloor);

    // walls[0] = rect top edge (0,0)->(10,0): horizontal, length 10.
    const world::Box& top = plan.walls[0];
    CHECK(approx(top.center.x, 5.0f));
    CHECK(approx(top.center.y, 1.5f));   // wallHeight/2
    CHECK(approx(top.center.z, 0.0f));
    CHECK(approx(top.size.x, 10.0f));    // length
    CHECK(approx(top.size.y, 3.0f));     // height
    CHECK(approx(top.size.z, 0.2f));     // thickness
    CHECK(approx(top.yaw, 0.0f));        // horizontal

    // walls[1] = rect right edge (10,0)->(10,8): vertical, length 8.
    const world::Box& right = plan.walls[1];
    CHECK(approx(right.center.x, 10.0f));
    CHECK(approx(right.center.z, 4.0f));
    CHECK(approx(right.size.x, 8.0f));
    CHECK(approx(right.yaw, std::atan2(8.0f, 0.0f))); // +pi/2

    // Door: midpoint of (3,8)->(5,8) = (4,8), length 2, horizontal.
    const world::Box& door = plan.doors[0];
    CHECK(approx(door.center.x, 4.0f));
    CHECK(approx(door.center.z, 8.0f));
    CHECK(approx(door.size.x, 2.0f));
    CHECK(approx(door.yaw, 0.0f));

    // Floor spans the bounding box (10 x 8), centered, just below y=0.
    CHECK(approx(plan.floor.size.x, 10.0f));
    CHECK(approx(plan.floor.size.z, 8.0f));
    CHECK(approx(plan.floor.center.x, 5.0f));
    CHECK(approx(plan.floor.center.z, 4.0f));
    CHECK(approx(plan.floor.center.y, -0.05f)); // -floorThickness/2

    // Emit into the ECS: one entity per wall + the floor (doors are openings).
    ecs::Registry world;
    const std::size_t created = world::emitToRegistry(plan, world);
    CHECK(created == 8); // 7 walls + floor
    CHECK(world.view<ecs::Transform>().count() == 8);

    // Scale of an emitted Transform equals the box size; rotation.y equals yaw.
    bool foundVertical = false;
    world.view<ecs::Transform>().each([&](ecs::Entity, ecs::Transform& t) {
        if (approx(t.scale.x, 8.0f) && approx(t.rotation.y, std::atan2(8.0f, 0.0f))) {
            foundVertical = true;
        }
    });
    CHECK(foundVertical);

    if (g_failures == 0) {
        std::printf("All SVG floor-plan importer tests passed.\n");
        return 0;
    }
    std::printf("%d SVG floor-plan importer test(s) failed.\n", g_failures);
    return 1;
}
