// Test for the cascaded shadow map (CSM) math - issue #236 (rendering P4).
//
// The GPU integration (depth texture array, per-cascade passes, and the shader's
// per-fragment cascade selection + seam blend) mirrors these functions, so testing
// them here verifies the split scheme, cascade selection, frustum fit, and ortho
// projection headlessly (GLSL is not compiled in CI, and the GL engine cannot run
// in this environment).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_shadow_cascades.cpp -o test_shadow_cascades

#include "render/ShadowCascades.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;
using IKore::pick::unproject;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

static void testSplitCountAndEndpoints() {
    const std::vector<float> s = cascadeSplitDistances(0.1f, 100.0f, 4, 0.5f);
    CHECK(s.size() == 5u); // count + 1 boundaries
    CHECK(approx(s.front(), 0.1f));   // pinned to near
    CHECK(approx(s.back(), 100.0f));  // pinned to far
    // Strictly increasing.
    for (std::size_t i = 1; i < s.size(); ++i) CHECK(s[i] > s[i - 1]);

    // Count clamps to at least one cascade (two boundaries).
    CHECK(cascadeSplitDistances(1.0f, 100.0f, 0, 0.5f).size() == 2u);
    CHECK(cascadeSplitDistances(1.0f, 100.0f, -3, 0.5f).size() == 2u);
    const std::vector<float> one = cascadeSplitDistances(1.0f, 100.0f, 1, 0.5f);
    CHECK(one.size() == 2u);
    CHECK(approx(one.front(), 1.0f));
    CHECK(approx(one.back(), 100.0f));
}

static void testSplitSchemes() {
    // lambda = 0 -> uniform spacing in depth.
    const std::vector<float> uni = cascadeSplitDistances(1.0f, 101.0f, 4, 0.0f);
    CHECK(approx(uni[1], 26.0f, 1e-2f));  // 1 + 100 * 0.25
    CHECK(approx(uni[2], 51.0f, 1e-2f));  // 1 + 100 * 0.50
    CHECK(approx(uni[3], 76.0f, 1e-2f));  // 1 + 100 * 0.75

    // lambda = 1 -> logarithmic spacing. near*(far/near)^0.5 = 1*sqrt(100) = 10.
    const std::vector<float> logd = cascadeSplitDistances(1.0f, 100.0f, 2, 1.0f);
    CHECK(approx(logd[1], 10.0f, 1e-2f));

    // lambda is clamped to [0, 1].
    const std::vector<float> hi = cascadeSplitDistances(1.0f, 101.0f, 4, 2.0f);
    const std::vector<float> lo = cascadeSplitDistances(1.0f, 101.0f, 4, -1.0f);
    for (int i = 1; i < 4; ++i) {
        CHECK(approx(hi[static_cast<std::size_t>(i)],
                     cascadeSplitDistances(1.0f, 101.0f, 4, 1.0f)[static_cast<std::size_t>(i)]));
        CHECK(approx(lo[static_cast<std::size_t>(i)],
                     cascadeSplitDistances(1.0f, 101.0f, 4, 0.0f)[static_cast<std::size_t>(i)]));
    }

    // Practical blend lies between the logarithmic and uniform splits.
    const std::vector<float> mix = cascadeSplitDistances(1.0f, 100.0f, 2, 0.5f);
    CHECK(mix[1] > 10.0f);     // above the pure-log split
    CHECK(mix[1] < 50.5f);     // below the pure-uniform split (1 + 99*0.5)
    CHECK(approx(mix[1], 0.5f * 10.0f + 0.5f * 50.5f, 1e-2f));
}

static void testSelectCascade() {
    // 3 cascades: [1,10], [10,50], [50,100].
    const std::vector<float> b{1.0f, 10.0f, 50.0f, 100.0f};
    CHECK(selectCascade(5.0f, b) == 0);
    CHECK(selectCascade(10.0f, b) == 0);   // upper boundary belongs to the lower cascade
    CHECK(selectCascade(10.1f, b) == 1);
    CHECK(selectCascade(50.0f, b) == 1);
    CHECK(selectCascade(50.1f, b) == 2);
    CHECK(selectCascade(100.0f, b) == 2);
    CHECK(selectCascade(999.0f, b) == 2); // beyond far clamps to the last cascade
    CHECK(selectCascade(0.5f, b) == 0);   // before near falls into cascade 0
    CHECK(selectCascade(-5.0f, b) == 0);

    // Degenerate boundary lists never index out of range.
    CHECK(selectCascade(5.0f, std::vector<float>{}) == 0);
    CHECK(selectCascade(5.0f, std::vector<float>{42.0f}) == 0);
}

static void testFrustumCornersIdentity() {
    // With an identity inverse view-projection, unproject is the identity, so the
    // corners are exactly the NDC cube corners.
    const std::array<Vec3, 8> c = frustumCornersWorld(Mat4::identity());
    for (const Vec3& v : c) {
        CHECK(approx(std::fabs(v.x), 1.0f));
        CHECK(approx(std::fabs(v.y), 1.0f));
        CHECK(approx(std::fabs(v.z), 1.0f));
    }
    // Loop order: first corner is (-1,-1,-1), last is (+1,+1,+1).
    CHECK(approx(c[0].x, -1.0f) && approx(c[0].y, -1.0f) && approx(c[0].z, -1.0f));
    CHECK(approx(c[7].x, 1.0f) && approx(c[7].y, 1.0f) && approx(c[7].z, 1.0f));
}

static void testLightSpaceBounds() {
    const std::array<Vec3, 8> corners = frustumCornersWorld(Mat4::identity());

    // Identity light view: bounds are the NDC cube.
    const Aabb id = lightSpaceBounds(corners, Mat4::identity());
    CHECK(approx(id.min.x, -1.0f) && approx(id.min.y, -1.0f) && approx(id.min.z, -1.0f));
    CHECK(approx(id.max.x, 1.0f) && approx(id.max.y, 1.0f) && approx(id.max.z, 1.0f));

    // Translated light view (column-major translation in m[12]) shifts the box.
    Mat4 trans = Mat4::identity();
    trans.m[12] = 10.0f;
    const Aabb t = lightSpaceBounds(corners, trans);
    CHECK(approx(t.min.x, 9.0f) && approx(t.max.x, 11.0f));
    CHECK(approx(t.min.y, -1.0f) && approx(t.max.y, 1.0f));

    // Scaled light view widens the box on that axis.
    Mat4 scale = Mat4::identity();
    scale.m[0] = 2.0f;
    const Aabb s = lightSpaceBounds(corners, scale);
    CHECK(approx(s.min.x, -2.0f) && approx(s.max.x, 2.0f));
}

static void testOrthoFromBounds() {
    // Symmetric box maps min -> -1, max -> +1, center -> 0.
    const Aabb box{Vec3{-2.0f, -4.0f, -6.0f}, Vec3{2.0f, 4.0f, 6.0f}};
    const Mat4 o = orthoFromBounds(box);
    const Vec3 lo = unproject(o, box.min.x, box.min.y, box.min.z);
    const Vec3 hi = unproject(o, box.max.x, box.max.y, box.max.z);
    const Vec3 mid = unproject(o, 0.0f, 0.0f, 0.0f);
    CHECK(approx(lo.x, -1.0f) && approx(lo.y, -1.0f) && approx(lo.z, -1.0f));
    CHECK(approx(hi.x, 1.0f) && approx(hi.y, 1.0f) && approx(hi.z, 1.0f));
    CHECK(approx(mid.x, 0.0f) && approx(mid.y, 0.0f) && approx(mid.z, 0.0f));

    // Asymmetric box: center (5,10,20) maps to the clip origin.
    const Aabb off{Vec3{0.0f, 0.0f, 0.0f}, Vec3{10.0f, 20.0f, 40.0f}};
    const Mat4 o2 = orthoFromBounds(off);
    const Vec3 c = unproject(o2, 5.0f, 10.0f, 20.0f);
    CHECK(approx(c.x, 0.0f) && approx(c.y, 0.0f) && approx(c.z, 0.0f));
    const Vec3 lo2 = unproject(o2, 0.0f, 0.0f, 0.0f);
    CHECK(approx(lo2.x, -1.0f) && approx(lo2.y, -1.0f) && approx(lo2.z, -1.0f));

    // Degenerate extent falls back to a unit span, producing finite output.
    const Aabb flat{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 5.0f, 5.0f}};
    const Mat4 o3 = orthoFromBounds(flat);
    const Vec3 f = unproject(o3, 0.0f, 5.0f, 5.0f);
    CHECK(std::isfinite(f.x) && std::isfinite(f.y) && std::isfinite(f.z));
}

static void testEndToEndUnitFrustum() {
    // A cascade whose light-space bounds are exactly the NDC cube yields an identity
    // ortho, so corners round-trip through the full pipeline unchanged.
    const std::array<Vec3, 8> corners = frustumCornersWorld(Mat4::identity());
    const Aabb bounds = lightSpaceBounds(corners, Mat4::identity());
    const Mat4 ortho = orthoFromBounds(bounds);
    for (const Vec3& v : corners) {
        const Vec3 clip = unproject(ortho, v.x, v.y, v.z);
        CHECK(approx(clip.x, v.x) && approx(clip.y, v.y) && approx(clip.z, v.z));
    }
}

int main() {
    testSplitCountAndEndpoints();
    testSplitSchemes();
    testSelectCascade();
    testFrustumCornersIdentity();
    testLightSpaceBounds();
    testOrthoFromBounds();
    testEndToEndUnitFrustum();

    if (g_failures == 0) {
        std::printf("All shadow cascade tests passed.\n");
        return 0;
    }
    std::printf("%d shadow cascade test(s) failed.\n", g_failures);
    return 1;
}
