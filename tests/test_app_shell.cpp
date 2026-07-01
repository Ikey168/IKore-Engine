// Test for the mobile app shell loop - Milestone 17, #172.
//
// The OpenGL ES renderer and native Android/iOS touch layer cannot run headless,
// so this covers the platform-agnostic shell that they drive: touch-stroke
// capture, and the draw -> snap -> play -> share loop over the converter library.
//
// Includes only the shell header; pure std + header-only:
//   g++ -std=c++17 -I src tests/test_app_shell.cpp -o test_app_shell

#include "mobile/AppShell.h"

#include <cstdio>
#include <string>

using namespace IKore;
using namespace IKore::mobile;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static void drawRoom(DrawCanvas& c) {
    c.addStroke(Stroke{{{20, 20}, {70, 20}}, 20, 20, 20, 2});
    c.addStroke(Stroke{{{70, 20}, {70, 70}}, 20, 20, 20, 2});
    c.addStroke(Stroke{{{70, 70}, {20, 70}}, 20, 20, 20, 2});
    c.addStroke(Stroke{{{20, 70}, {20, 20}}, 20, 20, 20, 2});
}

static void testTouchStrokeCapture() {
    DrawCanvas c(100, 100);
    c.onTouch({0, TouchPhase::Down, 10, 10});
    c.onTouch({0, TouchPhase::Move, 20, 20});
    c.onTouch({0, TouchPhase::Move, 30, 20});
    c.onTouch({0, TouchPhase::Up, 30, 20});
    CHECK(c.strokes.size() == 1);
    CHECK(c.strokes[0].points.size() == 4);

    // A second stroke is separate.
    c.onTouch({0, TouchPhase::Down, 50, 50});
    c.onTouch({0, TouchPhase::Up, 55, 55});
    CHECK(c.strokes.size() == 2);

    c.clear();
    CHECK(c.strokes.empty());

    // Rasterizes to a top-down image with dark ink on white paper.
    c.addStroke(Stroke{{{10, 10}, {40, 10}}, 20, 20, 20, 2});
    const cv::Image img = c.rasterize();
    CHECK(img.width == 100 && img.channels == 3);
    CHECK(img.luminance(25, 10) < 100.0f);  // on the stroke: dark
    CHECK(img.luminance(80, 80) > 200.0f);  // blank paper: light
}

static void testStartPlayIsGated() {
    AppShell shell(100, 100);
    CHECK(shell.state() == AppState::Draw);
    drawRoom(shell.canvas());

    shell.snap();
    CHECK(shell.state() == AppState::Review);
    CHECK(!shell.level().walls.empty()); // the drawing became walls

    // No start/exit yet: play is refused and the state stays in review.
    CHECK(!shell.startPlay());
    CHECK(shell.state() == AppState::Review);

    shell.level().addSymbol("player", doodle::Vec3{30, 0, 35});
    shell.level().addSymbol("exit", doodle::Vec3{60, 0, 35});
    CHECK(shell.startPlay());
    CHECK(shell.state() == AppState::Play);
}

static void testFullLoopDrawSnapPlayShare() {
    AppShell shell(100, 100);

    // Draw a room (via touch, then finalized strokes for the walls).
    shell.onDrawTouch({0, TouchPhase::Down, 20, 20});
    shell.onDrawTouch({0, TouchPhase::Up, 20, 20});
    drawRoom(shell.canvas());

    // Snap: interpret the drawing.
    shell.snap();
    CHECK(shell.state() == AppState::Review);

    // Review: place the required objects along an open lane.
    shell.level().addSymbol("player", doodle::Vec3{30, 0, 35});
    shell.level().addSymbol("coin", doodle::Vec3{45, 0, 35});
    shell.level().addSymbol("exit", doodle::Vec3{60, 0, 35});
    CHECK(shell.level().readyToPlay());

    // Play: drive the player to the right with touch input until the level is won.
    CHECK(shell.startPlay());
    CHECK(shell.game().hasExit);
    shell.setMoveInput(1.0f, 0.0f);
    int steps = 0;
    for (; steps < 900 && !shell.gameFinished(); ++steps) shell.update(1.0f / 60.0f);
    CHECK(shell.game().won());

    // Share: emit the level and verify it is a valid, reloadable map.
    shell.share();
    CHECK(shell.state() == AppState::Share);
    CHECK(!shell.sharePayload().empty());
    doodle::LevelSpec reloaded;
    CHECK(doodle::loadLevel(shell.sharePayload(), reloaded));
    CHECK(reloaded.symbols.size() == 3);

    // Start over.
    shell.newDrawing();
    CHECK(shell.state() == AppState::Draw);
    CHECK(shell.canvas().strokes.empty());
}

int main() {
    testTouchStrokeCapture();
    testStartPlayIsGated();
    testFullLoopDrawSnapPlayShare();

    if (g_failures == 0) {
        std::printf("All app shell tests passed.\n");
        return 0;
    }
    std::printf("%d app shell test(s) failed.\n", g_failures);
    return 1;
}
