// Test for the reusable UI/HUD framework core - Milestone 9, #55.
//
// Verifies the headless model behind the in-game HUD / menu screens: anchor
// resolution (all nine anchors), resolution + scale aware layout, and live data
// binding to game/ECS-style state. The ImGui panel that draws the elements lives
// in DebugUI (engine build).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_hud_framework.cpp -o test_hud_framework

#include "ui/HudFramework.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static bool posEq(HudVec2 p, float x, float y) { return approx(p.x, x) && approx(p.y, y); }

static void testAnchorResolution() {
    const HudVec2 screen{1000.0f, 600.0f};
    const HudVec2 size{100.0f, 40.0f};
    const HudVec2 off{10.0f, 20.0f};

    // Horizontal: left -> margin, center -> centered + shift, right -> margin from right.
    // Vertical:   top  -> margin, middle -> centered + shift, bottom -> margin from bottom.
    CHECK(posEq(resolveAnchor(HudAnchor::TopLeft, screen, size, off), 10.0f, 20.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::TopCenter, screen, size, off), 460.0f, 20.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::TopRight, screen, size, off), 890.0f, 20.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::MiddleLeft, screen, size, off), 10.0f, 300.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::Center, screen, size, off), 460.0f, 300.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::MiddleRight, screen, size, off), 890.0f, 300.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::BottomLeft, screen, size, off), 10.0f, 540.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::BottomCenter, screen, size, off), 460.0f, 540.0f));
    CHECK(posEq(resolveAnchor(HudAnchor::BottomRight, screen, size, off), 890.0f, 540.0f));
}

static void testResolutionAwareness() {
    // The same right/bottom anchored element stays a fixed margin from the edge as
    // the resolution changes - the hallmark of resolution-aware layout.
    const HudVec2 size{80.0f, 30.0f};
    const HudVec2 off{16.0f, 16.0f};
    const HudVec2 small = resolveAnchor(HudAnchor::BottomRight, {800.0f, 600.0f}, size, off);
    const HudVec2 big = resolveAnchor(HudAnchor::BottomRight, {1920.0f, 1080.0f}, size, off);
    CHECK(approx(800.0f - small.x - size.x, 16.0f));   // margin from right edge preserved
    CHECK(approx(1920.0f - big.x - size.x, 16.0f));
    CHECK(approx(600.0f - small.y - size.y, 16.0f));   // margin from bottom edge preserved
    CHECK(approx(1080.0f - big.y - size.y, 16.0f));
}

static void testScaleAffectsLayout() {
    Hud hud;
    hud.setScreenSize(1000.0f, 600.0f);
    hud.setScale(2.0f);
    CHECK(approx(hud.scale(), 2.0f));

    HudElement& top = hud.add(hudText("tl", HudAnchor::TopLeft, {10.0f, 20.0f}, {100.0f, 40.0f},
                                      [] { return std::string("x"); }));
    HudElement& corner = hud.add(hudValue("br", HudAnchor::BottomRight, {10.0f, 20.0f}, {100.0f, 40.0f},
                                          [] { return 0; }));
    HudElement& mid = hud.add(hudText("c", HudAnchor::Center, {0.0f, 0.0f}, {100.0f, 40.0f},
                                      [] { return std::string("x"); }));

    // Offsets scale: top-left margin doubles from (10,20) to (20,40).
    CHECK(posEq(hud.positionOf(top), 20.0f, 40.0f));
    // Size and offset scale: 100x40 -> 200x80, offset 10/20 -> 20/40.
    CHECK(posEq(hud.positionOf(corner), 1000.0f - 200.0f - 20.0f, 600.0f - 80.0f - 40.0f));
    // Centering uses the scaled size: (1000-200)/2, (600-80)/2.
    CHECK(posEq(hud.positionOf(mid), 400.0f, 260.0f));

    // Non-positive scale is ignored (stays 2.0).
    hud.setScale(0.0f);
    CHECK(approx(hud.scale(), 2.0f));
    hud.setScale(-1.0f);
    CHECK(approx(hud.scale(), 2.0f));
}

// A stand-in for live ECS/game state the HUD binds to.
struct PlayerState {
    int health{100};
    int maxHealth{100};
    int ammo{30};
    int score{0};
    std::vector<std::string> inventory;
};

static void testLiveBinding() {
    PlayerState player;
    player.inventory = {"Sword", "Shield"};

    Hud hud;
    hud.setScreenSize(1920.0f, 1080.0f);

    HudElement& health = hud.add(hudBar("Health", HudAnchor::TopLeft, {16.0f, 16.0f}, {220.0f, 22.0f},
                                        [&player] {
                                            return static_cast<float>(player.health) /
                                                   static_cast<float>(player.maxHealth);
                                        }));
    HudElement& ammo = hud.add(hudValue("Ammo", HudAnchor::BottomRight, {16.0f, 16.0f}, {120.0f, 26.0f},
                                        [&player] { return player.ammo; }));
    HudElement& score = hud.add(hudText("Score", HudAnchor::TopRight, {16.0f, 16.0f}, {180.0f, 26.0f},
                                        [&player] { return "Score: " + std::to_string(player.score); }));
    HudElement& inv = hud.add(hudList("Inventory", HudAnchor::BottomLeft, {16.0f, 16.0f}, {180.0f, 120.0f},
                                      [&player] { return player.inventory; }));

    CHECK(hud.count() == 4);

    // Initial reads reflect the bound state.
    CHECK(approx(health.bar(), 1.0f));
    CHECK(ammo.value() == 30);
    CHECK(score.text() == "Score: 0");
    CHECK(inv.list().size() == 2);

    // Mutating the game state is reflected live on the next read - no re-binding.
    player.health = 45;
    player.ammo = 12;
    player.score = 1500;
    player.inventory.push_back("Potion");
    CHECK(approx(health.bar(), 0.45f));
    CHECK(ammo.value() == 12);
    CHECK(score.text() == "Score: 1500");
    CHECK(inv.list().size() == 3 && inv.list().back() == "Potion");

    // The bar clamps to [0, 1] even when the raw ratio goes out of range.
    player.health = 130; // overheal
    CHECK(approx(health.bar(), 1.0f));
    player.health = -20; // dead / negative
    CHECK(approx(health.bar(), 0.0f));
}

static void testDefaultsAndVisibility() {
    HudElement blank; // no sources bound
    CHECK(blank.text().empty());
    CHECK(blank.value() == 0);
    CHECK(approx(blank.bar(), 0.0f));
    CHECK(blank.list().empty());
    CHECK(blank.visible);                       // visible by default
    CHECK(blank.anchor == HudAnchor::TopLeft);  // sensible default anchor

    Hud hud;
    CHECK(hud.count() == 0);
    hud.add(hudText("a", HudAnchor::Center, {}, {10.0f, 10.0f}, [] { return std::string("hi"); }));
    CHECK(hud.count() == 1);
    hud.clear();
    CHECK(hud.count() == 0);

    // Anchor names are stable for panel labels / debugging.
    CHECK(std::string(hudAnchorName(HudAnchor::BottomRight)) == "BottomRight");
    CHECK(std::string(hudAnchorName(HudAnchor::Center)) == "Center");
}

int main() {
    testAnchorResolution();
    testResolutionAwareness();
    testScaleAffectsLayout();
    testLiveBinding();
    testDefaultsAndVisibility();

    if (g_failures == 0) {
        std::printf("All HUD framework tests passed.\n");
        return 0;
    }
    std::printf("%d HUD framework test(s) failed.\n", g_failures);
    return 1;
}
