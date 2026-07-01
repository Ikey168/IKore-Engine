// Test for the header-only PBR BRDF math - issue #233 (rendering P2).
//
// Verifies the Cook-Torrance terms without a GL context: GGX distribution D
// (including hemisphere energy normalization), Smith geometry G, Fresnel-Schlick
// F, sRGB<->linear round-trip, and edge cases (roughness 0/1, grazing angles).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_pbr_brdf.cpp -o test_pbr_brdf

#include "render/PbrBrdf.h"

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

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

// 2*pi * integral over the upper hemisphere of D(theta) * cos(theta) * sin(theta).
// For a correctly normalized NDF this equals 1.
static double ggxHemisphereIntegral(float roughness) {
    const int n = 100000;
    const double halfPi = static_cast<double>(kPi) / 2.0;
    const double dTheta = halfPi / n;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double theta = (i + 0.5) * dTheta;
        const double nh = std::cos(theta);
        const double d = distributionGGX(static_cast<float>(nh), roughness);
        sum += d * nh * std::sin(theta) * dTheta;
    }
    return 2.0 * static_cast<double>(kPi) * sum;
}

static void testDistribution() {
    // D is non-negative and peaks when the half vector aligns with the normal.
    CHECK(distributionGGX(1.0f, 0.5f) > 0.0f);
    CHECK(distributionGGX(1.0f, 0.5f) > distributionGGX(0.5f, 0.5f));
    CHECK(distributionGGX(0.5f, 0.5f) > distributionGGX(0.1f, 0.5f));

    // Fully rough: alpha^2 = 1 so D = 1/pi for every direction.
    CHECK(approx(distributionGGX(1.0f, 1.0f), 1.0f / kPi));
    CHECK(approx(distributionGGX(0.3f, 1.0f), 1.0f / kPi));

    // Edge cases stay finite (roughness 0 must not divide by zero).
    CHECK(std::isfinite(distributionGGX(1.0f, 0.0f)));
    CHECK(std::isfinite(distributionGGX(0.0f, 0.0f)));
    CHECK(distributionGGX(1.0f, 0.0f) > distributionGGX(1.0f, 0.5f)); // sharper when smoother
}

static void testEnergyConservation() {
    // The GGX NDF integrates to 1 over the hemisphere (energy-conserving).
    CHECK(approx(static_cast<float>(ggxHemisphereIntegral(0.3f)), 1.0f, 0.02f));
    CHECK(approx(static_cast<float>(ggxHemisphereIntegral(0.5f)), 1.0f, 0.02f));
    CHECK(approx(static_cast<float>(ggxHemisphereIntegral(0.8f)), 1.0f, 0.02f));
}

static void testGeometry() {
    // Full facing: both G1 terms are 1, so G = 1.
    CHECK(approx(geometrySmith(1.0f, 1.0f, 0.5f), 1.0f));

    // G stays within [0, 1] and vanishes at grazing angles.
    const float g = geometrySmith(0.5f, 0.5f, 0.5f);
    CHECK(g > 0.0f && g < 1.0f);
    CHECK(approx(geometrySmith(0.0f, 1.0f, 0.5f), 0.0f)); // grazing view -> masked
    CHECK(approx(geometrySmith(1.0f, 0.0f, 0.5f), 0.0f)); // grazing light -> shadowed

    // Monotonic: more head-on is less occluded.
    CHECK(geometrySmith(0.9f, 0.9f, 0.5f) > geometrySmith(0.2f, 0.2f, 0.5f));

    // Roughness remaps: direct = (r+1)^2/8, IBL = r^2/2.
    CHECK(approx(roughnessToKDirect(1.0f), 4.0f / 8.0f));
    CHECK(approx(roughnessToKIBL(1.0f), 0.5f));
    CHECK(std::isfinite(geometrySmith(0.5f, 0.5f, 0.0f)));
    CHECK(std::isfinite(geometrySmith(0.5f, 0.5f, 1.0f)));
}

static void testFresnel() {
    // Normal incidence returns F0; grazing returns 1.
    CHECK(approx(fresnelSchlick(1.0f, 0.04f), 0.04f));
    CHECK(approx(fresnelSchlick(0.0f, 0.04f), 1.0f));

    // Never dips below F0 or exceeds 1 (no energy created), and rises toward grazing.
    const float mid = fresnelSchlick(0.5f, 0.04f);
    CHECK(mid >= 0.04f && mid <= 1.0f);
    CHECK(fresnelSchlick(0.2f, 0.04f) > fresnelSchlick(0.8f, 0.04f));

    // Known dielectric value: glass (IOR 1.5) has F0 = 0.04.
    CHECK(approx(fresnelF0FromIor(1.5f), 0.04f));
}

static void testSrgb() {
    // Endpoints and a known midpoint.
    CHECK(approx(srgbToLinear(0.0f), 0.0f));
    CHECK(approx(srgbToLinear(1.0f), 1.0f));
    CHECK(approx(srgbToLinear(0.5f), 0.21404f, 1e-3f));
    CHECK(approx(linearToSrgb(1.0f), 1.0f));

    // Round-trip linear -> sRGB -> linear across the range.
    const float samples[] = {0.0f, 0.02f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float v : samples) {
        CHECK(approx(srgbToLinear(linearToSrgb(v)), v, 1e-4f));
    }
}

int main() {
    testDistribution();
    testEnergyConservation();
    testGeometry();
    testFresnel();
    testSrgb();

    if (g_failures == 0) {
        std::printf("All PBR BRDF tests passed.\n");
        return 0;
    }
    std::printf("%d PBR BRDF test(s) failed.\n", g_failures);
    return 1;
}
