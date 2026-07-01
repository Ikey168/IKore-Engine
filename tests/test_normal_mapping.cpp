// Test for the tangent-space normal-mapping math - issue #238 (rendering P4).
//
// The Phong vertex/fragment shaders mirror these functions (Gram-Schmidt tangent,
// cross-product bitangent, normal unpack, TBN transform), so testing them here
// verifies the perturbed-normal math headlessly (GLSL is not compiled in CI and the
// GL engine cannot run in this environment). Also covers per-triangle tangent
// generation (the load-time fallback) and handedness.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_normal_mapping.cpp -o test_normal_mapping

#include "render/NormalMapping.h"

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
static bool approxVec(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

static void testDotCross() {
    CHECK(approx(dot(Vec3{1, 0, 0}, Vec3{1, 0, 0}), 1.0f));
    CHECK(approx(dot(Vec3{1, 0, 0}, Vec3{0, 1, 0}), 0.0f));
    CHECK(approxVec(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}), Vec3{0, 0, 1})); // right-handed
    CHECK(approxVec(cross(Vec3{0, 1, 0}, Vec3{0, 0, 1}), Vec3{1, 0, 0}));
}

static void testComputeTangentBasis() {
    // Triangle in the XY plane with U along +X and V along +Y.
    const TangentBasis tb = computeTangentBasis(
        Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0},
        Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1});
    CHECK(tb.valid);
    CHECK(approxVec(tb.tangent, Vec3{1, 0, 0}));   // +U -> +X
    CHECK(approxVec(tb.bitangent, Vec3{0, 1, 0})); // +V -> +Y

    // Scaling the U rate shortens the raw tangent but keeps its direction.
    const TangentBasis scaled = computeTangentBasis(
        Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0},
        Vec2{0, 0}, Vec2{2, 0}, Vec2{0, 1});
    CHECK(scaled.valid);
    CHECK(approxVec(scaled.tangent.normalized(), Vec3{1, 0, 0}));

    // A triangle whose U axis runs along world +Z.
    const TangentBasis zAxis = computeTangentBasis(
        Vec3{0, 0, 0}, Vec3{0, 0, 1}, Vec3{0, 1, 0},
        Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1});
    CHECK(zAxis.valid);
    CHECK(approxVec(zAxis.tangent.normalized(), Vec3{0, 0, 1}));

    // Degenerate UVs (zero area) mark the basis invalid instead of dividing by zero.
    const TangentBasis bad = computeTangentBasis(
        Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0},
        Vec2{0.5f, 0.5f}, Vec2{0.5f, 0.5f}, Vec2{0.5f, 0.5f});
    CHECK(!bad.valid);
    CHECK(std::isfinite(bad.tangent.x) && std::isfinite(bad.bitangent.y));
}

static void testOrthonormalizeTangent() {
    // Removing the normal-parallel part of the tangent leaves a unit vector in-plane.
    const Vec3 n{0, 0, 1};
    const Vec3 t = orthonormalizeTangent(n, Vec3{1, 0, 0.7f});
    CHECK(approxVec(t, Vec3{1, 0, 0}));
    CHECK(approx(t.length(), 1.0f));
    CHECK(approx(dot(t, n), 0.0f)); // orthogonal to the normal

    // An already-orthogonal tangent is just normalized.
    const Vec3 t2 = orthonormalizeTangent(Vec3{0, 0, 1}, Vec3{0, 2, 0});
    CHECK(approxVec(t2, Vec3{0, 1, 0}));
}

static void testBitangentAndSign() {
    const Vec3 n{0, 0, 1}, t{1, 0, 0};
    CHECK(approxVec(computeBitangent(n, t), Vec3{0, 1, 0})); // cross(N, T)

    // Right-handed basis -> +1; a flipped bitangent (mirrored UVs) -> -1.
    CHECK(approx(bitangentSign(n, t, Vec3{0, 1, 0}), 1.0f));
    CHECK(approx(bitangentSign(n, t, Vec3{0, -1, 0}), -1.0f));
}

static void testUnpackNormal() {
    // A flat normal texel decodes to (0, 0, 1).
    CHECK(approxVec(unpackNormal(Vec3{0.5f, 0.5f, 1.0f}), Vec3{0, 0, 1}));
    // Extremes decode to the axis extents.
    CHECK(approxVec(unpackNormal(Vec3{1.0f, 0.5f, 0.5f}), Vec3{1, 0, 0}));
    CHECK(approxVec(unpackNormal(Vec3{0.0f, 0.0f, 0.0f}), Vec3{-1, -1, -1}));
}

static void testTransformNormalToWorld() {
    const Vec3 t{1, 0, 0}, b{0, 1, 0}, n{0, 0, 1};

    // With an identity TBN the world normal equals the tangent-space normal.
    CHECK(approxVec(transformNormalToWorld(t, b, n, Vec3{0, 0, 1}), Vec3{0, 0, 1}));
    CHECK(approxVec(transformNormalToWorld(t, b, n, Vec3{1, 0, 0}), Vec3{1, 0, 0}));

    // Key property: a flat tangent-space normal maps to the geometric normal, so an
    // absent/flat normal map leaves shading unchanged. Use a rotated basis to prove
    // it is N (not just z) that comes back.
    const Vec3 rt{0, 0, 1}, rb{0, 1, 0}, rn{1, 0, 0};
    const Vec3 flat = unpackNormal(Vec3{0.5f, 0.5f, 1.0f}); // (0,0,1)
    CHECK(approxVec(transformNormalToWorld(rt, rb, rn, flat), Vec3{1, 0, 0})); // == rn

    // The result is always unit length.
    const Vec3 w = transformNormalToWorld(t, b, n, Vec3{0.3f, -0.4f, 0.8f});
    CHECK(approx(w.length(), 1.0f));

    // A tangent-space tilt toward +U rotates the world normal toward T.
    const Vec3 tilted = transformNormalToWorld(t, b, n, Vec3{0.6f, 0.0f, 0.8f});
    CHECK(tilted.x > 0.0f);        // leaned toward the tangent
    CHECK(tilted.z > 0.0f);        // still mostly along the normal
    CHECK(tilted.x < tilted.z);
}

static void testEndToEnd() {
    // Build a basis the way the vertex shader does, then perturb like the fragment
    // shader, and confirm a flat map reproduces the geometric normal exactly.
    const Vec3 geoNormal = Vec3{0.2f, 0.3f, 1.0f}.normalized();
    const Vec3 rawTangent{1.0f, 0.1f, 0.0f};
    const Vec3 T = orthonormalizeTangent(geoNormal, rawTangent);
    const Vec3 B = computeBitangent(geoNormal, T);

    const Vec3 flatWorld = transformNormalToWorld(T, B, geoNormal, unpackNormal(Vec3{0.5f, 0.5f, 1.0f}));
    CHECK(approxVec(flatWorld, geoNormal, 1e-3f));

    // T, B, N form an orthonormal frame.
    CHECK(approx(dot(T, geoNormal), 0.0f, 1e-3f));
    CHECK(approx(dot(T, B), 0.0f, 1e-3f));
    CHECK(approx(T.length(), 1.0f, 1e-3f));
    CHECK(approx(B.length(), 1.0f, 1e-3f));
}

int main() {
    testDotCross();
    testComputeTangentBasis();
    testOrthonormalizeTangent();
    testBitangentAndSign();
    testUnpackNormal();
    testTransformNormalToWorld();
    testEndToEnd();

    if (g_failures == 0) {
        std::printf("All normal mapping tests passed.\n");
        return 0;
    }
    std::printf("%d normal mapping test(s) failed.\n", g_failures);
    return 1;
}
