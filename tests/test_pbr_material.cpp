// Test for the opt-in PBR material + reference shading equation - issue #234.
//
// The PBR GLSL shader mirrors evaluateDirectionalPbr(), so testing the reference
// here verifies the shading math headlessly (GLSL is not compiled in CI). Covers
// the opt-in default, metallic vs dielectric response, roughness response, energy
// sanity, and grazing/horizon behavior.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_pbr_material.cpp -o test_pbr_material

#include "render/PbrMaterial.h"

#include <cmath>
#include <cstdio>

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

static bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
static bool finite3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static void testOptInDefault() {
    PbrMaterial m;
    CHECK(!m.enabled); // materials are Phong unless they opt in
    CHECK(approx(m.metallic, 0.0f));
    CHECK(approx(m.roughness, 0.5f));
    CHECK(approx(m.ao, 1.0f));
    CHECK(approx(m.albedo.x, 1.0f) && approx(m.albedo.y, 1.0f) && approx(m.albedo.z, 1.0f));
}

static void testDielectricHeadOn() {
    // Normal, view, and light all aligned with +Z (head-on).
    const Vec3 up{0.0f, 0.0f, 1.0f};
    const Vec3 white{1.0f, 1.0f, 1.0f};
    PbrMaterial red;
    red.albedo = Vec3{0.8f, 0.0f, 0.0f};
    red.metallic = 0.0f;
    red.roughness = 0.5f;

    const Vec3 lo = evaluateDirectionalPbr(up, up, up, white, red);
    CHECK(finite3(lo));
    CHECK(lo.x > 0.0f);
    // A red dielectric reflects a small gray (F0 = 0.04) specular into all channels,
    // so green/blue are positive but far below the red diffuse.
    CHECK(lo.y > 0.0f);
    CHECK(lo.x > lo.y);
    CHECK(approx(lo.y, lo.z)); // specular is gray for a dielectric
}

static void testMetalVsDielectric() {
    const Vec3 up{0.0f, 0.0f, 1.0f};
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 redAlbedo{0.8f, 0.0f, 0.0f};

    PbrMaterial metal;
    metal.albedo = redAlbedo;
    metal.metallic = 1.0f;
    metal.roughness = 0.3f;

    PbrMaterial dielectric;
    dielectric.albedo = redAlbedo;
    dielectric.metallic = 0.0f;
    dielectric.roughness = 0.3f;

    const Vec3 loMetal = evaluateDirectionalPbr(up, up, up, white, metal);
    const Vec3 loDielectric = evaluateDirectionalPbr(up, up, up, white, dielectric);

    // A metal has no diffuse and its specular is tinted by albedo, so a red metal
    // reflects essentially no green; a red dielectric shows gray specular in green.
    CHECK(approx(loMetal.y, 0.0f, 1e-4f));
    CHECK(loDielectric.y > loMetal.y);
    CHECK(loMetal.x > 0.0f); // red channel still reflects
}

static void testHorizonAndGrazing() {
    const Vec3 n{0.0f, 0.0f, 1.0f};
    const Vec3 v{0.0f, 0.0f, 1.0f};
    const Vec3 sideLight{1.0f, 0.0f, 0.0f}; // perpendicular to the normal
    const Vec3 white{1.0f, 1.0f, 1.0f};
    PbrMaterial m;
    m.albedo = Vec3{1.0f, 1.0f, 1.0f};

    const Vec3 lo = evaluateDirectionalPbr(n, v, sideLight, white, m);
    CHECK(approx(lo.x, 0.0f) && approx(lo.y, 0.0f) && approx(lo.z, 0.0f)); // NdotL = 0

    // A light behind the surface also contributes nothing.
    const Vec3 behind{0.0f, 0.0f, -1.0f};
    const Vec3 loBehind = evaluateDirectionalPbr(n, v, behind, white, m);
    CHECK(approx(loBehind.x, 0.0f));
}

static void testEnergySanity() {
    // White dielectric under white light, head-on: output stays finite and modest
    // (no energy explosion). Fully rough is diffuse-dominated (~albedo/pi).
    const Vec3 up{0.0f, 0.0f, 1.0f};
    const Vec3 white{1.0f, 1.0f, 1.0f};
    PbrMaterial m;
    m.albedo = Vec3{1.0f, 1.0f, 1.0f};
    m.metallic = 0.0f;
    m.roughness = 1.0f;

    const Vec3 lo = evaluateDirectionalPbr(up, up, up, white, m);
    CHECK(finite3(lo));
    CHECK(lo.x > 0.0f && lo.x < 1.5f);
    CHECK(approx(lo.x, lo.y) && approx(lo.y, lo.z)); // white in, white out
}

static void testRoughnessResponse() {
    // Smoother metal has a sharper (stronger) specular highlight head-on.
    const Vec3 up{0.0f, 0.0f, 1.0f};
    const Vec3 white{1.0f, 1.0f, 1.0f};
    PbrMaterial smooth;
    smooth.albedo = Vec3{1.0f, 1.0f, 1.0f};
    smooth.metallic = 1.0f;
    smooth.roughness = 0.1f;
    PbrMaterial rough = smooth;
    rough.roughness = 0.9f;

    const Vec3 loSmooth = evaluateDirectionalPbr(up, up, up, white, smooth);
    const Vec3 loRough = evaluateDirectionalPbr(up, up, up, white, rough);
    CHECK(finite3(loSmooth) && finite3(loRough));
    CHECK(loSmooth.x > loRough.x);

    // Edge roughness values stay finite.
    PbrMaterial r0 = smooth;
    r0.roughness = 0.0f;
    PbrMaterial r1 = smooth;
    r1.roughness = 1.0f;
    CHECK(finite3(evaluateDirectionalPbr(up, up, up, white, r0)));
    CHECK(finite3(evaluateDirectionalPbr(up, up, up, white, r1)));
}

int main() {
    testOptInDefault();
    testDielectricHeadOn();
    testMetalVsDielectric();
    testHorizonAndGrazing();
    testEnergySanity();
    testRoughnessResponse();

    if (g_failures == 0) {
        std::printf("All PBR material tests passed.\n");
        return 0;
    }
    std::printf("%d PBR material test(s) failed.\n", g_failures);
    return 1;
}
