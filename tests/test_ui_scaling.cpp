// Test for the DPI / resolution UI scaling core - Milestone 9, #61.
//
// Verifies the effective-scale formula (user * DPI * resolution auto-scale, with
// clamping and DPI-fallback) and aspect-ratio classification used for adaptive
// layout. The application of the scale (ImGui font/style, HUD) lives in DebugUI.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_ui_scaling.cpp -o test_ui_scaling

#include "ui/UiScaling.h"

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

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static void testBaselineAndResolutionAutoScale() {
    UiScaleConfig cfg; // reference 1080, user 1, auto on, clamp [0.5, 3]

    // At the reference resolution with DPI 1, the scale is 1.
    CHECK(approx(computeUiScale(cfg, 1080.0f, 1.0f), 1.0f));

    // 1440p and 4K scale up proportionally.
    CHECK(approx(computeUiScale(cfg, 1440.0f, 1.0f), 1440.0f / 1080.0f));
    CHECK(approx(computeUiScale(cfg, 2160.0f, 1.0f), 2.0f));

    // 720p scales down.
    CHECK(approx(computeUiScale(cfg, 720.0f, 1.0f), 720.0f / 1080.0f));
}

static void testUserScaleAndDpi() {
    UiScaleConfig cfg;

    // User scale multiplies through at the reference resolution.
    cfg.userScale = 1.5f;
    CHECK(approx(computeUiScale(cfg, 1080.0f, 1.0f), 1.5f));

    // DPI multiplies too.
    cfg.userScale = 1.0f;
    CHECK(approx(computeUiScale(cfg, 1080.0f, 2.0f), 2.0f));

    // DPI <= 0 is treated as 1.0.
    CHECK(approx(computeUiScale(cfg, 1080.0f, 0.0f), 1.0f));
    CHECK(approx(computeUiScale(cfg, 1080.0f, -3.0f), 1.0f));
}

static void testClamping() {
    UiScaleConfig cfg; // clamp [0.5, 3]

    // Combined factors exceed the max -> clamped.
    cfg.userScale = 1.5f;
    CHECK(approx(computeUiScale(cfg, 2160.0f, 2.0f), 3.0f)); // 1.5 * 2 * 2 = 6 -> 3

    // Tiny resolution with a small user scale clamps to the min.
    cfg.userScale = 0.5f;
    CHECK(approx(computeUiScale(cfg, 720.0f, 1.0f), 0.5f)); // 0.5 * 0.667 = 0.333 -> 0.5

    // Custom clamps are honored.
    UiScaleConfig tight;
    tight.minScale = 1.0f;
    tight.maxScale = 1.25f;
    CHECK(approx(computeUiScale(tight, 480.0f, 1.0f), 1.0f));  // would be < 1 -> min
    CHECK(approx(computeUiScale(tight, 2160.0f, 1.0f), 1.25f)); // would be 2 -> max
}

static void testAutoResolutionScaleToggle() {
    UiScaleConfig cfg;
    cfg.autoResolutionScale = false;

    // With auto-scale off, resolution height is ignored: only user * DPI.
    CHECK(approx(computeUiScale(cfg, 2160.0f, 1.0f), 1.0f));
    CHECK(approx(computeUiScale(cfg, 720.0f, 1.0f), 1.0f));
    cfg.userScale = 1.25f;
    CHECK(approx(computeUiScale(cfg, 4320.0f, 1.0f), 1.25f));
}

static void testAspectRatioAndClassification() {
    CHECK(approx(aspectRatio(1920.0f, 1080.0f), 16.0f / 9.0f));
    CHECK(approx(aspectRatio(800.0f, 600.0f), 4.0f / 3.0f));
    CHECK(approx(aspectRatio(100.0f, 0.0f), 0.0f)); // guard against divide-by-zero

    CHECK(classifyAspect(1080.0f, 1920.0f) == AspectCategory::Tall);     // portrait
    CHECK(classifyAspect(1024.0f, 768.0f) == AspectCategory::Standard);  // 4:3
    CHECK(classifyAspect(1920.0f, 1080.0f) == AspectCategory::Wide);     // 16:9
    CHECK(classifyAspect(3440.0f, 1440.0f) == AspectCategory::UltraWide); // 21:9

    CHECK(std::string(aspectCategoryName(AspectCategory::UltraWide)) == "UltraWide");
    CHECK(std::string(aspectCategoryName(AspectCategory::Standard)) == "Standard");
}

int main() {
    testBaselineAndResolutionAutoScale();
    testUserScaleAndDpi();
    testClamping();
    testAutoResolutionScaleToggle();
    testAspectRatioAndClassification();

    if (g_failures == 0) {
        std::printf("All UI scaling tests passed.\n");
        return 0;
    }
    std::printf("%d UI scaling test(s) failed.\n", g_failures);
    return 1;
}
