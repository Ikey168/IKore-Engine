// Test for the ray-based viewport picking core - Milestone 9, #57.
//
// Verifies the headless picking math and state machine: screen-to-ray
// unprojection, ray/AABB and ray/sphere intersection, nearest-hit selection, and
// the Picker's hover/select behavior including the "UI consumes the click" gate.
// The cursor drawing and camera live in the engine (DebugUI / main).
//
// Pure std + header-only (reuses the glm-free ecs::Vec3):
//   g++ -std=c++17 -I src tests/test_picking.cpp -o test_picking

#include "core/Picking.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::pick;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static Ray downRay(float x, float z) { return Ray{Vec3{x, 100.0f, z}, Vec3{0.0f, -1.0f, 0.0f}}; }

static void testScreenToRayIdentity() {
    const Mat4 id = Mat4::identity();

    // Center of an 800x600 viewport -> NDC (0,0). With an identity inverse VP the
    // near plane sits at z=-1 and the ray points down +Z.
    const Ray center = screenToRay(400.0f, 300.0f, 800.0f, 600.0f, id);
    CHECK(approx(center.origin.x, 0.0f) && approx(center.origin.y, 0.0f) && approx(center.origin.z, -1.0f));
    CHECK(approx(center.dir.x, 0.0f) && approx(center.dir.y, 0.0f) && approx(center.dir.z, 1.0f));

    // Top-right pixel -> NDC (+1, +1) (screen y flips).
    const Ray corner = screenToRay(800.0f, 0.0f, 800.0f, 600.0f, id);
    CHECK(approx(corner.origin.x, 1.0f) && approx(corner.origin.y, 1.0f));

    // Bottom-left pixel -> NDC (-1, -1).
    const Ray bl = screenToRay(0.0f, 600.0f, 800.0f, 600.0f, id);
    CHECK(approx(bl.origin.x, -1.0f) && approx(bl.origin.y, -1.0f));
}

static void testScreenToRayTranslation() {
    // Inverse VP that translates by (5, -2, 10): the unprojected points shift, so
    // the ray origin moves with the camera while the direction stays +Z.
    Mat4 t = Mat4::identity();
    t.m[12] = 5.0f;
    t.m[13] = -2.0f;
    t.m[14] = 10.0f;

    const Ray r = screenToRay(400.0f, 300.0f, 800.0f, 600.0f, t);
    CHECK(approx(r.origin.x, 5.0f) && approx(r.origin.y, -2.0f) && approx(r.origin.z, 9.0f));
    CHECK(approx(r.dir.z, 1.0f));
}

static void testRayAabb() {
    const Ray r{Vec3{0.0f, 0.0f, -5.0f}, Vec3{0.0f, 0.0f, 1.0f}};

    float t = 0.0f;
    const Aabb atOrigin = Aabb::fromCenterHalf(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    CHECK(rayAabb(r, atOrigin, t));
    CHECK(approx(t, 4.0f)); // enters the box at z = -1

    // Offset in X so the +Z ray misses.
    const Aabb offset = Aabb::fromCenterHalf(Vec3{5.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    CHECK(!rayAabb(r, offset, t));

    // A box entirely behind the origin is not hit (only t >= 0 counts).
    const Ray away{Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    CHECK(!rayAabb(away, atOrigin, t));

    // Ray parallel to a slab and outside it: miss.
    const Ray parallel{Vec3{5.0f, 0.0f, -5.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    CHECK(!rayAabb(parallel, atOrigin, t));
}

static void testRaySphere() {
    const Ray r{Vec3{0.0f, 0.0f, -5.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    float t = 0.0f;
    CHECK(raySphere(r, Vec3{0.0f, 0.0f, 0.0f}, 1.0f, t));
    CHECK(approx(t, 4.0f));

    CHECK(!raySphere(r, Vec3{5.0f, 0.0f, 0.0f}, 1.0f, t)); // off to the side, misses
}

static void testPickNearest() {
    // Two boxes straight ahead; the nearer one wins.
    std::vector<Pickable> items = {
        {1, Aabb::fromCenterHalf(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f})},
        {2, Aabb::fromCenterHalf(Vec3{0.0f, 0.0f, -5.0f}, Vec3{1.0f, 1.0f, 1.0f})},
    };
    const Ray r{Vec3{0.0f, 0.0f, -10.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    const PickResult res = pickNearest(r, items);
    CHECK(res.hit);
    CHECK(res.id == 2); // the box at z=-5 is closer to the origin at z=-10
    CHECK(approx(res.distance, 4.0f));

    // A ray that hits nothing.
    const Ray miss{Vec3{0.0f, 50.0f, -10.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    CHECK(!pickNearest(miss, items).hit);
}

static void testPickerHoverAndSelect() {
    std::vector<Pickable> items = {
        {10, Aabb::fromCenterHalf(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.5f, 0.5f, 0.5f})},
        {20, Aabb::fromCenterHalf(Vec3{3.0f, 0.0f, 0.0f}, Vec3{0.5f, 0.5f, 0.5f})},
    };
    Picker picker;

    // Hover over the first box (top-down ray at x=0), no click yet.
    picker.update(downRay(0.0f, 0.0f), items, /*pointerOverUI=*/false, /*clicked=*/false);
    CHECK(picker.hasHover());
    CHECK(picker.hovered() == 10);
    CHECK(!picker.hasSelection());

    // Click selects the hovered entity (feeds the inspector).
    picker.update(downRay(0.0f, 0.0f), items, false, true);
    CHECK(picker.hasSelection());
    CHECK(picker.selected() == 10);

    // Move over the second box and click: selection follows.
    picker.update(downRay(3.0f, 0.0f), items, false, true);
    CHECK(picker.selected() == 20);

    // Click on empty space clears the selection.
    picker.update(downRay(50.0f, 0.0f), items, false, true);
    CHECK(!picker.hasHover());
    CHECK(!picker.hasSelection());
}

static void testPickerUIConsumesClick() {
    std::vector<Pickable> items = {
        {10, Aabb::fromCenterHalf(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.5f, 0.5f, 0.5f})},
    };
    Picker picker;

    // Establish a selection via the scene.
    picker.update(downRay(0.0f, 0.0f), items, false, true);
    CHECK(picker.selected() == 10);

    // Now the pointer is over the UI: even a click that geometrically hits the box
    // must not reach the scene - hover clears and the selection is untouched.
    picker.update(downRay(0.0f, 0.0f), items, /*pointerOverUI=*/true, /*clicked=*/true);
    CHECK(!picker.hasHover());
    CHECK(picker.hasSelection());
    CHECK(picker.selected() == 10);

    CHECK(sceneShouldReceiveMouse(false) == true);
    CHECK(sceneShouldReceiveMouse(true) == false);
}

int main() {
    testScreenToRayIdentity();
    testScreenToRayTranslation();
    testRayAabb();
    testRaySphere();
    testPickNearest();
    testPickerHoverAndSelect();
    testPickerUIConsumesClick();

    if (g_failures == 0) {
        std::printf("All picking tests passed.\n");
        return 0;
    }
    std::printf("%d picking test(s) failed.\n", g_failures);
    return 1;
}
