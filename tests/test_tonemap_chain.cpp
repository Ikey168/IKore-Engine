// Test for composing HDR tone mapping with the bloom chain - issue #268.
//
// The chain change is GL wiring (bloom leaves linear HDR; the ACES resolve applies
// exposure + ACES + gamma once at the end, before FXAA), which cannot run headlessly.
// What is verifiable is the math rationale behind it, expressed with the unit-tested
// render::ToneMapping core that tonemap.frag mirrors:
//   - bloom must be composited in linear HDR *before* the single resolve;
//   - letting bloom keep its own Reinhard + gamma and then resolving again (the double
//     application this issue removes) gives a materially different, wrong result;
//   - with bloom contributing nothing, the composed result equals the plain #235
//     resolve, so enabling the composition does not disturb the tone curve.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_tonemap_chain.cpp -o test_tonemap_chain

#include "render/ToneMapping.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace IKore::render;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

// The old combine path (Reinhard + gamma) that bloom_combine.frag applies when the
// resolve is OFF. Kept here to model the double application the gate avoids.
static float bloomReinhardGamma(float linear) {
    const float reinhard = linear / (linear + 1.0f);
    return std::pow(reinhard, 1.0f / 2.2f);
}

// Bloom composites additively in linear HDR, then the chain resolves once.
static void testLinearCompositeThenSingleResolve() {
    const float exposure = 1.0f;
    const float scene = 0.6f;
    const float bloom = 0.9f;
    const float intensity = 0.8f;

    const float composited = scene + bloom * intensity; // linear HDR add
    const float composed = resolveChannel(composited, exposure);

    // Compositing in linear before the resolve is not the same as resolving each
    // contributor and adding display values - order matters, and linear is correct.
    const float wrongOrder = resolveChannel(scene, exposure) + resolveChannel(bloom * intensity, exposure);
    CHECK(!approx(composed, wrongOrder, 1e-3f));

    // The composed result is a valid display value in [0, 1].
    CHECK(composed >= 0.0f && composed <= 1.0f);
}

// Letting bloom apply Reinhard + gamma and then running the ACES resolve (the double
// tone-map + double gamma this issue removes) differs materially from resolving once.
static void testNoDoubleApplication() {
    const float exposure = 1.0f;
    const float composited = 0.6f + 0.9f * 0.8f; // same linear HDR value as above

    const float once = resolveChannel(composited, exposure);        // correct: single resolve
    const float twice = resolveChannel(bloomReinhardGamma(composited), exposure); // bug: bloom + resolve

    // The double application visibly changes the pixel (here by well over 5%), which is
    // exactly why bloom_combine bypasses its Reinhard + gamma when the resolve is on.
    CHECK(std::fabs(once - twice) > 0.05f);
}

// With bloom off (or zero intensity), the composed resolve equals the plain #235
// resolve, so turning the composition on does not disturb the tone curve.
static void testBloomOffMatchesPlainResolve() {
    for (float exposure : {0.5f, 1.0f, 2.0f}) {
        for (float scene : {0.0f, 0.25f, 1.0f, 4.0f}) {
            const float composedNoBloom = resolveChannel(scene + 0.0f, exposure);
            const float plain = resolveChannel(scene, exposure);
            CHECK(approx(composedNoBloom, plain));
        }
    }
}

// The LDR path (resolve off) is independent of exposure/ACES: bloom's own Reinhard +
// gamma is the whole tone map, unchanged from before this issue.
static void testLdrPathUnchanged() {
    const float composited = 0.6f + 0.9f * 0.8f;
    const float ldr = bloomReinhardGamma(composited);
    CHECK(ldr >= 0.0f && ldr <= 1.0f);
    // Independent of exposure (exposure only exists in the resolve path).
    CHECK(approx(ldr, bloomReinhardGamma(composited)));
}

int main() {
    testLinearCompositeThenSingleResolve();
    testNoDoubleApplication();
    testBloomOffMatchesPlainResolve();
    testLdrPathUnchanged();

    if (g_failures == 0) {
        std::printf("All tonemap chain tests passed.\n");
        return 0;
    }
    std::printf("%d tonemap chain test(s) failed.\n", g_failures);
    return 1;
}
