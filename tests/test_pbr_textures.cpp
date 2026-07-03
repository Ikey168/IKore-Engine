// Test for the PBR texture/factor blending rule - issue #269.
//
// resolvePbrInputs() combines the scalar material factors with sampled albedo,
// metallic-roughness, and AO texture channels the glTF way (factor * texture, with a
// missing texture passing 1). src/shaders/pbr.frag mirrors it, so testing it here
// locks the texture/uniform blending headlessly (GLSL is not compiled in CI) and
// keeps the CPU reference and the shader in agreement.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_pbr_textures.cpp -o test_pbr_textures

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

static bool approx(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

// With no textures (all channels 1), the resolved inputs equal the scalar factors.
static void testNoTexturesUsesFactors() {
    PbrMaterial m;
    m.enabled = true;
    m.albedo = Vec3{0.8f, 0.4f, 0.2f};
    m.metallic = 0.6f;
    m.roughness = 0.3f;
    m.ao = 0.9f;

    const PbrMaterial r = resolvePbrInputs(m, Vec3{1.0f, 1.0f, 1.0f}, 1.0f, 1.0f, 1.0f);
    CHECK(r.enabled);
    CHECK(approx(r.albedo.x, 0.8f) && approx(r.albedo.y, 0.4f) && approx(r.albedo.z, 0.2f));
    CHECK(approx(r.metallic, 0.6f));
    CHECK(approx(r.roughness, 0.3f));
    CHECK(approx(r.ao, 0.9f));
}

// With unit factors, the resolved inputs equal the texture samples (glTF channels:
// roughness in G, metallic in B, AO in R).
static void testUnitFactorsUsesTextures() {
    PbrMaterial m;
    m.albedo = Vec3{1.0f, 1.0f, 1.0f};
    m.metallic = 1.0f;
    m.roughness = 1.0f;
    m.ao = 1.0f;

    const Vec3 albedoTexel{0.5f, 0.25f, 0.125f};
    const float roughnessG = 0.7f;
    const float metallicB = 0.2f;
    const float aoR = 0.6f;
    const PbrMaterial r = resolvePbrInputs(m, albedoTexel, roughnessG, metallicB, aoR);

    CHECK(approx(r.albedo.x, 0.5f) && approx(r.albedo.y, 0.25f) && approx(r.albedo.z, 0.125f));
    CHECK(approx(r.roughness, 0.7f)); // G channel
    CHECK(approx(r.metallic, 0.2f));  // B channel
    CHECK(approx(r.ao, 0.6f));        // R channel
}

// Factor and texture combine multiplicatively (the glTF rule).
static void testMultiplicativeBlend() {
    PbrMaterial m;
    m.albedo = Vec3{0.8f, 0.8f, 0.8f};
    m.metallic = 0.5f;
    m.roughness = 0.4f;
    m.ao = 0.5f;

    const PbrMaterial r = resolvePbrInputs(m, Vec3{0.5f, 0.5f, 0.5f}, 0.5f, 0.4f, 0.2f);
    CHECK(approx(r.albedo.x, 0.4f)); // 0.8 * 0.5
    CHECK(approx(r.roughness, 0.2f)); // 0.4 * 0.5 (G)
    CHECK(approx(r.metallic, 0.2f));  // 0.5 * 0.4 (B)
    CHECK(approx(r.ao, 0.1f));        // 0.5 * 0.2 (R)
}

// The resolved inputs feed the tested shading equation unchanged: an all-white AO
// texture leaves ambient response identical to the scalar-only path.
static void testResolvedFeedsShading() {
    PbrMaterial factors;
    factors.enabled = true;
    factors.albedo = Vec3{0.9f, 0.1f, 0.1f};
    factors.metallic = 0.2f;
    factors.roughness = 0.5f;
    factors.ao = 1.0f;

    const PbrMaterial noTex = resolvePbrInputs(factors, Vec3{1, 1, 1}, 1.0f, 1.0f, 1.0f);
    const Vec3 N{0, 0, 1}, V{0, 0, 1}, L{0, 0, 1}, radiance{1, 1, 1};
    const Vec3 a = evaluateDirectionalPbr(N, V, L, radiance, factors);
    const Vec3 b = evaluateDirectionalPbr(N, V, L, radiance, noTex);
    CHECK(approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z));
}

int main() {
    testNoTexturesUsesFactors();
    testUnitFactorsUsesTextures();
    testMultiplicativeBlend();
    testResolvedFeedsShading();

    if (g_failures == 0) {
        std::printf("All PBR texture blending tests passed.\n");
        return 0;
    }
    std::printf("%d PBR texture blending test(s) failed.\n", g_failures);
    return 1;
}
