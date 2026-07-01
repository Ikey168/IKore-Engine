// Test for the PCF soft-shadow filtering math - issue #237 (rendering P4).
//
// The lighting shader's ShadowCalculation mirrors these functions (same tap sets,
// same per-tap depth comparison, same average), so testing them here verifies the
// soft-shadow math headlessly (GLSL is not compiled in CI and the GL engine cannot
// run in this environment). Covers the depth comparison, texel size, box and
// Poisson tap sets, the averaged filter, softness scaling, and the single-tap case
// that reproduces the hard-shadow path.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_shadow_filtering.cpp -o test_shadow_filtering

#include "render/ShadowFiltering.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

static void testShadowTest() {
    // Occluded when the fragment is deeper than the stored depth (past the bias).
    CHECK(approx(shadowTest(0.8f, 0.5f, 0.0f), 1.0f));
    // Lit when the fragment is at or in front of the stored depth.
    CHECK(approx(shadowTest(0.4f, 0.5f, 0.0f), 0.0f));
    // Bias pulls a just-occluded fragment back into the light (fights acne).
    CHECK(approx(shadowTest(0.505f, 0.5f, 0.0f), 1.0f));
    CHECK(approx(shadowTest(0.505f, 0.5f, 0.01f), 0.0f));
}

static void testTexelSize() {
    CHECK(approx(shadowTexelSize(1024), 1.0f / 1024.0f));
    CHECK(approx(shadowTexelSize(2048), 1.0f / 2048.0f));
    // A smaller map has larger texels (a coarser, softer footprint per tap).
    CHECK(shadowTexelSize(512) > shadowTexelSize(2048));
    // Degenerate resolution never divides by zero.
    CHECK(approx(shadowTexelSize(0), 0.0f));
    CHECK(approx(shadowTexelSize(-4), 0.0f));
}

static void testBoxOffsets() {
    // Kernel 1 is a single origin tap (the hard-shadow sample).
    const std::vector<Offset2D> k1 = pcfBoxOffsets(1);
    CHECK(k1.size() == 1u);
    CHECK(approx(k1[0].x, 0.0f) && approx(k1[0].y, 0.0f));

    // Odd kernels give (2h+1)^2 centered taps.
    CHECK(pcfBoxOffsets(3).size() == 9u);
    CHECK(pcfBoxOffsets(5).size() == 25u);
    CHECK(pcfBoxOffsets(7).size() == 49u);

    // Sizes below 1 clamp to a single tap.
    CHECK(pcfBoxOffsets(0).size() == 1u);
    CHECK(pcfBoxOffsets(-3).size() == 1u);

    // The grid is symmetric and includes the origin.
    const std::vector<Offset2D> k3 = pcfBoxOffsets(3);
    bool hasOrigin = false;
    float sumX = 0.0f, sumY = 0.0f;
    for (const Offset2D& o : k3) {
        if (approx(o.x, 0.0f) && approx(o.y, 0.0f)) hasOrigin = true;
        sumX += o.x;
        sumY += o.y;
    }
    CHECK(hasOrigin);
    CHECK(approx(sumX, 0.0f) && approx(sumY, 0.0f)); // symmetric about the origin
}

static void testPoissonDisk() {
    const std::vector<Offset2D> disk = poissonDiskOffsets();
    CHECK(disk.size() == 16u);

    float sumX = 0.0f, sumY = 0.0f;
    for (const Offset2D& o : disk) {
        const float len = std::sqrt(o.x * o.x + o.y * o.y);
        CHECK(len <= 1.3f);     // bounded within a small disk
        CHECK(len > 0.05f);     // no degenerate zero-length tap
        sumX += o.x;
        sumY += o.y;
    }
    // Roughly centered on the origin (well distributed, not biased to one side).
    CHECK(std::fabs(sumX / 16.0f) < 0.1f);
    CHECK(std::fabs(sumY / 16.0f) < 0.1f);
}

static void testPcfFilterExtremes() {
    const std::vector<Offset2D> k3 = pcfBoxOffsets(3);
    const float texel = 1.0f / 1024.0f;

    // Everything nearer than the fragment -> fully occluded.
    const float allShadow = pcfFilter(0.5f, 0.5f, 0.8f, 0.001f, texel, 1.0f, k3,
                                      [](float, float) { return 0.0f; });
    CHECK(approx(allShadow, 1.0f));

    // Everything behind the fragment -> fully lit.
    const float allLit = pcfFilter(0.5f, 0.5f, 0.2f, 0.001f, texel, 1.0f, k3,
                                   [](float, float) { return 1.0f; });
    CHECK(approx(allLit, 0.0f));

    // Result is always a valid fraction.
    CHECK(allShadow >= 0.0f && allShadow <= 1.0f);
    CHECK(allLit >= 0.0f && allLit <= 1.0f);
}

static void testPcfFilterPartial() {
    // A half-plane occluder: taps left of center are occluded, the rest lit. For a
    // 3x3 kernel that is the x = -1 column (3 of 9 taps) -> 1/3 in shadow.
    const std::vector<Offset2D> k3 = pcfBoxOffsets(3);
    const float u = 0.5f, texel = 0.01f;
    const float frac = pcfFilter(u, 0.5f, 1.0f, 0.0f, texel, 1.0f, k3,
                                 [u, texel](float su, float) {
                                     return su < u - 0.5f * texel ? 0.0f : 2.0f;
                                 });
    CHECK(approx(frac, 3.0f / 9.0f, 1e-3f));
}

static void testHardPathEquivalence() {
    // A single-tap kernel must equal a plain shadowTest at the center (this is the
    // preserved hard-shadow path).
    const std::vector<Offset2D> k1 = pcfBoxOffsets(1);
    for (float d : {0.0f, 0.3f, 0.6f, 1.0f}) {
        const float filtered = pcfFilter(0.5f, 0.5f, 0.5f, 0.002f, 0.001f, 1.0f, k1,
                                         [d](float, float) { return d; });
        CHECK(approx(filtered, shadowTest(0.5f, d, 0.002f)));
    }
}

static void testSoftnessWidensFootprint() {
    // Occluder ring: a tap is occluded only once its offset carries it past 0.5
    // texel from center. Larger softness pushes taps outward, so more fall into the
    // occluded region -> a larger shadow fraction.
    const std::vector<Offset2D> k3 = pcfBoxOffsets(3);
    const float u = 0.5f, texel = 1.0f; // unit texel keeps the arithmetic clean
    auto ring = [u, texel](float su, float) {
        return std::fabs(su - u) > 0.5f * texel ? 0.0f : 2.0f; // 0 = occluder
    };
    const float tight = pcfFilter(u, 0.5f, 1.0f, 0.0f, texel, 0.1f, k3, ring);
    const float wide = pcfFilter(u, 0.5f, 1.0f, 0.0f, texel, 1.0f, k3, ring);
    CHECK(approx(tight, 0.0f));      // all taps stay within 0.05 texel -> lit
    CHECK(wide > tight);             // wider footprint reaches the occluder
    CHECK(approx(wide, 6.0f / 9.0f, 1e-3f)); // x = -1 and x = +1 columns occluded
}

static void testPoissonFilter() {
    const std::vector<Offset2D> disk = poissonDiskOffsets();
    const float texel = 1.0f / 2048.0f;
    const float allShadow = pcfFilter(0.5f, 0.5f, 0.9f, 0.001f, texel, 1.0f, disk,
                                      [](float, float) { return 0.0f; });
    const float allLit = pcfFilter(0.5f, 0.5f, 0.1f, 0.001f, texel, 1.0f, disk,
                                   [](float, float) { return 1.0f; });
    CHECK(approx(allShadow, 1.0f));
    CHECK(approx(allLit, 0.0f));

    // Empty tap set is treated as fully lit rather than dividing by zero.
    CHECK(approx(pcfFilter(0.5f, 0.5f, 0.9f, 0.0f, texel, 1.0f, std::vector<Offset2D>{},
                           [](float, float) { return 0.0f; }),
                 0.0f));
}

int main() {
    testShadowTest();
    testTexelSize();
    testBoxOffsets();
    testPoissonDisk();
    testPcfFilterExtremes();
    testPcfFilterPartial();
    testHardPathEquivalence();
    testSoftnessWidensFootprint();
    testPoissonFilter();

    if (g_failures == 0) {
        std::printf("All shadow filtering tests passed.\n");
        return 0;
    }
    std::printf("%d shadow filtering test(s) failed.\n", g_failures);
    return 1;
}
