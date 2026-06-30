// Test for the first-person "tour" camera mode - Milestone 15, #165.
//
// Verifies the issue's acceptance criterion: the player can switch to a
// first-person walkthrough of their map. Covers mode switching, look angles with
// pitch clamping, movement relative to the look direction, and that the
// walkthrough collides with walls (stays inside the level).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_tour_camera.cpp -o test_tour_camera

#include "game/DoodleScene.h"
#include "game/DungeonGame.h"
#include "game/TourCamera.h"

#include <cmath>
#include <cstdio>

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

static DungeonGame roomWithPlayer(float px, float pz) {
    LevelSpec spec;
    Wall room;
    room.polyline = {{0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10}, {0, 0, 0}};
    spec.walls.push_back(room);
    spec.symbols = {{"player", {px, 0, pz}, 0.0f}};
    return loadGame(convert(spec));
}

static void testModeSwitch() {
    const DungeonGame game = roomWithPlayer(3, 4);
    TourController tc;
    CHECK(!tc.isFirstPerson()); // starts top-down

    tc.enterFirstPerson(game);
    CHECK(tc.isFirstPerson());
    CHECK(approx(tc.camera.position.x, 3.0f));         // eye at the player
    CHECK(approx(tc.camera.position.z, 4.0f));
    CHECK(approx(tc.camera.position.y, tc.camera.eyeHeight));

    tc.exitToTopDown();
    CHECK(!tc.isFirstPerson());

    tc.toggle(game); // top-down -> first person
    CHECK(tc.isFirstPerson());
    tc.toggle(game); // and back
    CHECK(!tc.isFirstPerson());
}

static void testLookClampsPitch() {
    FirstPersonCamera cam;
    const float yaw0 = cam.yaw;
    cam.look(100.0f, 0.0f);
    CHECK(cam.yaw > yaw0); // turning right changes yaw

    cam.look(0.0f, 1e6f); // look way up
    CHECK(approx(cam.pitch, cam.pitchLimit));
    cam.look(0.0f, -1e7f); // look way down
    CHECK(approx(cam.pitch, -cam.pitchLimit));
}

static void testLookVectors() {
    FirstPersonCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    ecs::Vec3 f = cam.forward();
    CHECK(approx(f.x, 1.0f) && approx(f.y, 0.0f) && approx(f.z, 0.0f)); // +X

    cam.yaw = 3.14159265f / 2.0f;
    f = cam.forward();
    CHECK(approx(f.x, 0.0f) && approx(f.z, 1.0f)); // +Z

    cam.pitch = 0.5f;
    CHECK(cam.forward().y > 0.0f); // looking up

    // Horizontal basis is unit length and orthogonal.
    const ecs::Vec3 fh = cam.forwardHoriz();
    const ecs::Vec3 rh = cam.rightHoriz();
    CHECK(approx(fh.length(), 1.0f) && approx(rh.length(), 1.0f));
    CHECK(approx(fh.x * rh.x + fh.z * rh.z, 0.0f));
}

static void testWalkRelativeToLook() {
    const DungeonGame game = roomWithPlayer(5, 5);
    TourController tc;
    tc.enterFirstPerson(game);

    // Facing +X, walk forward -> x increases, z and eye height steady.
    tc.camera.yaw = 0.0f;
    const ecs::Vec3 start = tc.camera.position;
    for (int i = 0; i < 30; ++i) tc.walk(game, 1.0f, 0.0f, 1.0f / 60.0f);
    CHECK(tc.camera.position.x > start.x + 0.5f);
    CHECK(approx(tc.camera.position.z, start.z));
    CHECK(approx(tc.camera.position.y, tc.camera.eyeHeight));

    // Facing +Z, walk forward -> z increases.
    tc.camera.position = ecs::Vec3{5, tc.camera.eyeHeight, 5};
    tc.camera.yaw = 3.14159265f / 2.0f;
    for (int i = 0; i < 30; ++i) tc.walk(game, 1.0f, 0.0f, 1.0f / 60.0f);
    CHECK(tc.camera.position.z > 5.5f);

    // Walking is a no-op while top-down.
    tc.exitToTopDown();
    const ecs::Vec3 held = tc.camera.position;
    tc.walk(game, 1.0f, 0.0f, 1.0f / 60.0f);
    CHECK(approx(tc.camera.position.x, held.x) && approx(tc.camera.position.z, held.z));
}

static void testWalkthroughStaysInsideWalls() {
    const DungeonGame game = roomWithPlayer(5, 5);
    TourController tc;
    tc.enterFirstPerson(game);
    tc.camera.yaw = 0.0f; // face the +X wall at x=10

    bool everInsideWall = false;
    for (int i = 0; i < 600; ++i) {
        tc.walk(game, 1.0f, 0.0f, 1.0f / 60.0f);
        if (game.hitsWall(tc.camera.position, tc.radius)) everInsideWall = true;
    }
    CHECK(!everInsideWall);             // collision kept the camera out of walls
    CHECK(tc.camera.position.x < 9.7f); // blocked before passing through the wall
    CHECK(tc.camera.position.x > 5.0f); // but it did walk toward it
}

int main() {
    testModeSwitch();
    testLookClampsPitch();
    testLookVectors();
    testWalkRelativeToLook();
    testWalkthroughStaysInsideWalls();

    if (g_failures == 0) {
        std::printf("All tour camera tests passed.\n");
        return 0;
    }
    std::printf("%d tour camera test(s) failed.\n", g_failures);
    return 1;
}
