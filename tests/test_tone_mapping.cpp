// Test for the HDR exposure + ACES tone-mapping math - issue #235 (rendering P3).
//
// The resolve fragment shader mirrors these functions, so testing them here
// verifies the tone curve headlessly (GLSL is not compiled in CI). Covers the ACES
// curve shape (monotonic, 0->0, saturates to 1 without clipping), exposure control,
// and the per-channel resolve.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_tone_mapping.cpp -o test_tone_mapping

#include "render/ToneMapping.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace IKore::render;
using Vec3 = IKore::ecs::Vec3;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

static void testAcesCurveShape() {
    CHECK(approx(acesFilmic(0.0f), 0.0f)); // black stays black
    CHECK(acesFilmic(0.18f) > 0.0f);

    // Monotonic increasing.
    CHECK(acesFilmic(0.1f) < acesFilmic(0.5f));
    CHECK(acesFilmic(0.5f) < acesFilmic(1.0f));
    CHECK(acesFilmic(1.0f) < acesFilmic(4.0f));

    // Always within [0, 1] and saturates (no clipping/overflow) for bright input.
    CHECK(acesFilmic(1000.0f) <= 1.0f);
    CHECK(approx(acesFilmic(1000.0f), 1.0f, 1e-2f));
    CHECK(acesFilmic(1e9f) <= 1.0f);

    // Negative input clamps to black.
    CHECK(approx(acesFilmic(-5.0f), 0.0f));

    // Known value: ACES(1.0) = 2.54 / 3.16 ~= 0.8038.
    CHECK(approx(acesFilmic(1.0f), 0.8038f, 2e-3f));
}

static void testExposure() {
    CHECK(approx(applyExposure(0.5f, 2.0f), 1.0f));

    // More exposure brightens a mid tone (until it saturates).
    const float dim = resolveChannel(0.2f, 0.5f);
    const float mid = resolveChannel(0.2f, 1.0f);
    const float bright = resolveChannel(0.2f, 4.0f);
    CHECK(dim < mid);
    CHECK(mid < bright);
}

static void testResolveChannel() {
    // Output is always displayable [0, 1].
    for (float hdr : {0.0f, 0.1f, 0.5f, 1.0f, 5.0f, 50.0f}) {
        const float v = resolveChannel(hdr, 1.0f);
        CHECK(v >= 0.0f && v <= 1.0f);
    }
    CHECK(approx(resolveChannel(0.0f, 1.0f), 0.0f)); // black -> black
    // A very bright pixel resolves near white, not above it.
    CHECK(resolveChannel(50.0f, 1.0f) > 0.9f);
}

static void testResolveColor() {
    const Vec3 hdr{2.0f, 0.5f, 0.1f};
    const Vec3 out = resolveColor(hdr, 1.0f);
    CHECK(out.x >= 0.0f && out.x <= 1.0f);
    CHECK(out.y >= 0.0f && out.y <= 1.0f);
    CHECK(out.z >= 0.0f && out.z <= 1.0f);
    // Brighter input channel resolves to a brighter output channel.
    CHECK(out.x > out.y);
    CHECK(out.y > out.z);
    // Per-channel consistency with resolveChannel.
    CHECK(approx(out.x, resolveChannel(hdr.x, 1.0f)));
    CHECK(approx(out.z, resolveChannel(hdr.z, 1.0f)));
}

int main() {
    testAcesCurveShape();
    testExposure();
    testResolveChannel();
    testResolveColor();

    if (g_failures == 0) {
        std::printf("All tone mapping tests passed.\n");
        return 0;
    }
    std::printf("%d tone mapping test(s) failed.\n", g_failures);
    return 1;
}
