// Test for the SSAO kernel/occlusion math - issue #259 (renderer completions).
//
// src/shaders/ssao.frag mirrors these functions (kernel layout, range falloff,
// per-sample occlusion test, final resolve), so testing them here verifies the AO
// response headlessly (GLSL is not compiled in CI). Also locks the deterministic
// kernel/noise generation the PostProcessor now uploads.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_ssao_kernel.cpp -o test_ssao_kernel

#include "render/SsaoKernel.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;
using IKore::ecs::Vec3;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

static void testKernelDeterminismAndShape() {
    const std::vector<Vec3> a = generateSsaoKernel(64, 7);
    const std::vector<Vec3> b = generateSsaoKernel(64, 7);
    const std::vector<Vec3> c = generateSsaoKernel(64, 8);
    CHECK(a.size() == 64u);
    // Same seed -> identical kernel; different seed -> different kernel.
    bool identical = true, differs = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) identical = false;
        if (a[i].x != c[i].x || a[i].y != c[i].y || a[i].z != c[i].z) differs = true;
    }
    CHECK(identical);
    CHECK(differs);

    // Hemisphere: every sample has z >= 0 and fits inside the unit hemisphere.
    for (const Vec3& s : a) {
        CHECK(s.z >= 0.0f);
        CHECK(s.length() <= 1.0f + 1e-4f);
    }

    // The scale ramp clusters early samples near the origin: the first quarter is
    // shorter than the last quarter on average.
    float early = 0.0f, late = 0.0f;
    for (int i = 0; i < 16; ++i) early += a[static_cast<std::size_t>(i)].length();
    for (int i = 48; i < 64; ++i) late += a[static_cast<std::size_t>(i)].length();
    CHECK(early < late);

    CHECK(generateSsaoKernel(0).empty());
}

static void testScaleRamp() {
    CHECK(approx(ssaoScaleRamp(0, 64), 0.1f));
    // Monotonically increasing toward 1.
    float prev = -1.0f;
    for (int i = 0; i <= 64; ++i) {
        const float s = ssaoScaleRamp(i, 64);
        CHECK(s >= prev);
        CHECK(s >= 0.1f && s <= 1.0f + 1e-6f);
        prev = s;
    }
    CHECK(approx(ssaoScaleRamp(64, 64), 1.0f));
}

static void testNoiseAndScale() {
    const std::vector<Vec3> n1 = generateSsaoNoise(5);
    const std::vector<Vec3> n2 = generateSsaoNoise(5);
    CHECK(n1.size() == 16u);
    for (std::size_t i = 0; i < n1.size(); ++i) {
        CHECK(n1[i].z == 0.0f); // rotations stay in the tangent XY plane
        CHECK(n1[i].x >= -1.0f && n1[i].x <= 1.0f);
        CHECK(n1[i].y >= -1.0f && n1[i].y <= 1.0f);
        CHECK(n1[i].x == n2[i].x && n1[i].y == n2[i].y); // deterministic
    }

    float sx = 0, sy = 0;
    ssaoNoiseScale(1280, 720, sx, sy);
    CHECK(approx(sx, 320.0f) && approx(sy, 180.0f)); // the old hardcoded constant
    ssaoNoiseScale(800, 600, sx, sy);
    CHECK(approx(sx, 200.0f) && approx(sy, 150.0f)); // now resolution-correct
}

static void testSmoothstepAndRangeCheck() {
    CHECK(approx(smoothstep01(0.0f, 1.0f, -0.5f), 0.0f));
    CHECK(approx(smoothstep01(0.0f, 1.0f, 0.0f), 0.0f));
    CHECK(approx(smoothstep01(0.0f, 1.0f, 0.5f), 0.5f));
    CHECK(approx(smoothstep01(0.0f, 1.0f, 1.0f), 1.0f));
    CHECK(approx(smoothstep01(0.0f, 1.0f, 2.0f), 1.0f));

    // Occluder within the radius: full weight. Far beyond it: fades toward zero.
    CHECK(approx(ssaoRangeCheck(0.5f, -5.0f, -5.2f), 1.0f)); // dz 0.2 <= radius
    const float far1 = ssaoRangeCheck(0.5f, -5.0f, -6.0f);   // dz 1.0
    const float far2 = ssaoRangeCheck(0.5f, -5.0f, -15.0f);  // dz 10.0
    CHECK(far1 < 1.0f);
    CHECK(far2 < far1);
    CHECK(far2 < 0.05f);
}

static void testSampleOcclusionAndResolve() {
    const float radius = 0.5f, bias = 0.025f, fragZ = -5.0f;

    // Depth surface IN FRONT of the kernel sample point (view looks down -Z, so a
    // larger Z is closer): occluded, full range weight.
    CHECK(approx(ssaoSampleOcclusion(-4.9f, -5.1f, bias, radius, fragZ), 1.0f));
    // Surface BEHIND the sample point: not occluded.
    CHECK(approx(ssaoSampleOcclusion(-5.3f, -5.1f, bias, radius, fragZ), 0.0f));
    // Equal depths: the bias suppresses self-occlusion acne.
    CHECK(approx(ssaoSampleOcclusion(-5.1f, -5.1f, bias, radius, fragZ), 0.0f));
    // A far foreground object occludes but is range-attenuated to almost nothing.
    const float farOcclusion = ssaoSampleOcclusion(-25.0f + 30.0f, -5.1f, bias, radius, fragZ);
    CHECK(farOcclusion < 0.05f);

    // Resolve: no occlusion -> fully lit; everything occluded -> black.
    CHECK(approx(ssaoResolve(0.0f, 64), 1.0f));
    CHECK(approx(ssaoResolve(64.0f, 64), 0.0f));
    CHECK(approx(ssaoResolve(32.0f, 64), 0.5f));
    CHECK(approx(ssaoResolve(0.0f, 0), 1.0f)); // degenerate count

    // Combine weight: intensity 0 leaves the image untouched; 1 applies full AO;
    // out-of-range intensity clamps.
    CHECK(approx(ssaoCombineWeight(0.3f, 0.0f), 1.0f));
    CHECK(approx(ssaoCombineWeight(0.3f, 1.0f), 0.3f));
    CHECK(approx(ssaoCombineWeight(0.3f, 0.5f), 0.65f));
    CHECK(approx(ssaoCombineWeight(0.3f, 5.0f), 0.3f));
    CHECK(approx(ssaoCombineWeight(0.3f, -2.0f), 1.0f));
}

int main() {
    testKernelDeterminismAndShape();
    testScaleRamp();
    testNoiseAndScale();
    testSmoothstepAndRangeCheck();
    testSampleOcclusionAndResolve();

    if (g_failures == 0) {
        std::printf("All SSAO kernel tests passed.\n");
        return 0;
    }
    std::printf("%d SSAO kernel test(s) failed.\n", g_failures);
    return 1;
}
