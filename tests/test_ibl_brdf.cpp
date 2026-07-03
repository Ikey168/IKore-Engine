// Test for the image-based-lighting split-sum math - issue #270.
//
// The GPU generates the irradiance map, prefiltered specular mips, and the BRDF LUT
// by convolving the skybox cubemap; the sampling/weighting math is pure and lives in
// render::IblBrdf, mirrored by the generation shaders (brdf_lut.frag / prefilter.frag).
// GLSL is not compiled in CI and the GL passes cannot run here, so this locks the
// low-discrepancy sequence, the GGX importance sample, the split-sum BRDF integration
// LUT, and the mip <-> roughness mapping headlessly.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_ibl_brdf.cpp -o test_ibl_brdf

#include "render/IblBrdf.h"

#include <cmath>
#include <cstdio>

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

// Radical inverse and Hammersley: known values and range.
static void testHammersley() {
    CHECK(approx(radicalInverseVdC(0u), 0.0f));
    CHECK(approx(radicalInverseVdC(1u), 0.5f));      // bit 0 -> 0.5
    CHECK(approx(radicalInverseVdC(2u), 0.25f));     // bit 1 -> 0.25
    CHECK(approx(radicalInverseVdC(3u), 0.75f));     // 0.5 + 0.25

    const Vec2f h0 = hammersley(0u, 16u);
    CHECK(approx(h0.x, 0.0f) && approx(h0.y, 0.0f));
    const Vec2f h8 = hammersley(8u, 16u);
    CHECK(approx(h8.x, 0.5f));                        // 8/16
    // Every point lies in the unit square.
    for (std::uint32_t i = 0; i < 64; ++i) {
        const Vec2f h = hammersley(i, 64u);
        CHECK(h.x >= 0.0f && h.x < 1.0f);
        CHECK(h.y >= 0.0f && h.y < 1.0f);
    }
}

// Importance sampling: mirror surface concentrates H at the normal (+Z); rougher
// surfaces tilt it away.
static void testImportanceSampleGGX() {
    // At xi.y = 0 the half-vector is exactly the normal regardless of roughness.
    const Vec3f mirror = importanceSampleGGX(Vec2f{0.3f, 0.0f}, 0.5f);
    CHECK(approx(mirror.z, 1.0f, 1e-4f));

    // A returned half-vector is unit length and in the upper hemisphere.
    for (std::uint32_t i = 1; i < 32; ++i) {
        const Vec2f xi = hammersley(i, 32u);
        const Vec3f h = importanceSampleGGX(xi, 0.7f);
        CHECK(approx(h.x * h.x + h.y * h.y + h.z * h.z, 1.0f, 1e-3f));
        CHECK(h.z >= 0.0f);
    }

    // Smoother surfaces keep the sample closer to the normal (larger average z).
    float smoothZ = 0.0f, roughZ = 0.0f;
    for (std::uint32_t i = 0; i < 64; ++i) {
        const Vec2f xi = hammersley(i, 64u);
        smoothZ += importanceSampleGGX(xi, 0.05f).z;
        roughZ += importanceSampleGGX(xi, 0.95f).z;
    }
    CHECK(smoothZ > roughZ);
}

// The split-sum BRDF LUT (scale, bias): valid range, smooth-surface limit, energy.
static void testIntegrateBrdfLut() {
    // Head-on, very smooth: scale ~ 1, bias ~ 0 (specular reflectance ~ F0).
    const Vec2f headOnSmooth = integrateBrdfLut(1.0f, 0.02f, 2048u);
    CHECK(headOnSmooth.x > 0.9f);
    CHECK(headOnSmooth.y < 0.05f);

    // Everywhere: both terms in [0, 1] and finite, and scale + bias <= ~1 (no energy
    // gain: reflectance for F0 in [0,1] never exceeds 1).
    for (float ndv = 0.05f; ndv <= 1.0f; ndv += 0.15f) {
        for (float r = 0.0f; r <= 1.0f; r += 0.2f) {
            const Vec2f lut = integrateBrdfLut(ndv, r, 512u);
            CHECK(std::isfinite(lut.x) && std::isfinite(lut.y));
            CHECK(lut.x >= 0.0f && lut.x <= 1.0001f);
            CHECK(lut.y >= 0.0f && lut.y <= 1.0001f);
            CHECK(lut.x + lut.y <= 1.02f);
        }
    }

    // Deterministic: same inputs -> same output (precompute is stable).
    const Vec2f a = integrateBrdfLut(0.5f, 0.5f, 256u);
    const Vec2f b = integrateBrdfLut(0.5f, 0.5f, 256u);
    CHECK(approx(a.x, b.x) && approx(a.y, b.y));

    // At grazing angles the Fresnel-edge bias is strongest for smooth surfaces and
    // falls off as roughness scatters it, so bias decreases with roughness at low NdotV.
    const Vec2f grazSmooth = integrateBrdfLut(0.1f, 0.1f, 2048u);
    const Vec2f grazRough = integrateBrdfLut(0.1f, 0.9f, 2048u);
    CHECK(grazSmooth.y > grazRough.y);

    // Head-on, the bias term is ~0 across the roughness range (no Fresnel edge).
    CHECK(integrateBrdfLut(1.0f, 0.1f, 2048u).y < 0.02f);
    CHECK(integrateBrdfLut(1.0f, 0.9f, 2048u).y < 0.02f);
}

// Mip <-> roughness mapping is a linear round trip across the prefiltered chain.
static void testMipRoughnessMapping() {
    const int maxMip = 5;
    CHECK(approx(mipToRoughness(0, maxMip), 0.0f));
    CHECK(approx(mipToRoughness(maxMip, maxMip), 1.0f));
    CHECK(approx(mipToRoughness(2, maxMip), 0.4f));

    // roughnessToMip is the inverse scaling used by textureLod.
    CHECK(approx(roughnessToMip(0.0f, maxMip), 0.0f));
    CHECK(approx(roughnessToMip(1.0f, maxMip), 5.0f));
    CHECK(approx(roughnessToMip(0.4f, maxMip), 2.0f));

    // Degenerate chain does not divide by zero.
    CHECK(approx(mipToRoughness(3, 0), 0.0f));
}

int main() {
    testHammersley();
    testImportanceSampleGGX();
    testIntegrateBrdfLut();
    testMipRoughnessMapping();

    if (g_failures == 0) {
        std::printf("All IBL BRDF tests passed.\n");
        return 0;
    }
    std::printf("%d IBL BRDF test(s) failed.\n", g_failures);
    return 1;
}
