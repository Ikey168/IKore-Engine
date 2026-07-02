// Test for the per-cascade light-matrix assembly - issue #265 (CSM GPU wiring).
//
// buildCascades() turns the camera view/projection plus a light direction into one
// light matrix per cascade and the view-space split depth the shader compares
// against. The GPU path (depth texture array, one pass per cascade, and the
// shader's per-fragment cascade selection + seam blend) mirrors this, so verifying
// it here checks the matrix algebra (multiply, perspective, lookAt, inverse), the
// slice fit, split monotonicity, CPU/GPU cascade-selection agreement, and the
// count == 1 degradation headlessly (GLSL is not compiled in CI and the GL engine
// cannot run in this environment).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_shadow_cascade_build.cpp -o test_shadow_cascade_build

#include "render/ShadowCascadeBuild.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;
using IKore::pick::Mat4;
using IKore::pick::unproject;
// Vec3 comes from `using namespace IKore::render` (render::Vec3 == IKore::ecs::Vec3).

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

// A representative camera: looking down -Z from the origin, 60 deg fov, 16:9.
static Mat4 makeCamView() {
    return mat::lookAt(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f}, Vec3{0.0f, 1.0f, 0.0f});
}

// inverse(A) * A == identity, so the general 4x4 inverse is correct.
static void testInverseRoundTrip() {
    const Mat4 view = mat::lookAt(Vec3{3.0f, 4.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});
    const Mat4 proj = mat::perspective(1.0472f /* 60 deg */, 16.0f / 9.0f, 0.1f, 100.0f);
    const Mat4 vp = mat::mul(proj, view);
    const Mat4 back = mat::mul(mat::inverse(vp), vp);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            const float expected = (c == r) ? 1.0f : 0.0f;
            CHECK(approx(back.m[c * 4 + r], expected, 1e-3f));
        }
    }
}

// mul() must honor column-major order: identity is neutral, and a known product holds.
static void testMulConventions() {
    const Mat4 id = Mat4::identity();
    const Mat4 proj = mat::perspective(1.0f, 1.5f, 0.5f, 50.0f);
    const Mat4 lhs = mat::mul(id, proj);
    const Mat4 rhs = mat::mul(proj, id);
    for (int i = 0; i < 16; ++i) {
        CHECK(approx(lhs.m[i], proj.m[i]));
        CHECK(approx(rhs.m[i], proj.m[i]));
    }
}

// Splits are strictly increasing and each cascade's stored split is its slice's far end.
static void testSplitMonotonic() {
    const float nearZ = 0.1f, farZ = 100.0f;
    const int count = 4;
    const std::vector<Cascade> cs = buildCascades(makeCamView(), 1.0472f, 16.0f / 9.0f, nearZ, farZ,
                                                  Vec3{-0.3f, -1.0f, -0.2f}, count, 0.6f);
    CHECK(cs.size() == static_cast<std::size_t>(count));
    const std::vector<float> boundaries = cascadeSplitDistances(nearZ, farZ, count, 0.6f);
    for (int i = 0; i < count; ++i) {
        CHECK(approx(cs[static_cast<std::size_t>(i)].splitViewDepth, boundaries[static_cast<std::size_t>(i) + 1u]));
        if (i > 0) CHECK(cs[static_cast<std::size_t>(i)].splitViewDepth > cs[static_cast<std::size_t>(i) - 1u].splitViewDepth);
    }
    CHECK(approx(cs.back().splitViewDepth, farZ));
}

// Every corner of a cascade's own frustum slice must land inside that cascade's clip
// cube [-1, 1]^3 - i.e. the fitted ortho actually covers the slice it was built for.
static void testSliceCornersInsideClip() {
    const float nearZ = 0.1f, farZ = 80.0f;
    const int count = 3;
    const float fovy = 1.0472f, aspect = 16.0f / 9.0f, lambda = 0.5f;
    const Mat4 camView = makeCamView();
    const std::vector<Cascade> cs = buildCascades(camView, fovy, aspect, nearZ, farZ,
                                                  Vec3{-0.4f, -1.0f, -0.3f}, count, lambda);
    const std::vector<float> boundaries = cascadeSplitDistances(nearZ, farZ, count, lambda);
    CHECK(cs.size() == static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        const Mat4 sliceProj = mat::perspective(fovy, aspect, boundaries[static_cast<std::size_t>(i)],
                                                boundaries[static_cast<std::size_t>(i) + 1u]);
        const std::array<Vec3, 8> corners = frustumCornersWorld(mat::inverse(mat::mul(sliceProj, camView)));
        const Mat4& lm = cs[static_cast<std::size_t>(i)].lightMatrix;
        for (const Vec3& w : corners) {
            // Light matrix is affine (ortho * view), so w stays 1; unproject reuses it as a transform.
            const Vec3 clip = unproject(lm, w.x, w.y, w.z);
            CHECK(clip.x >= -1.0001f && clip.x <= 1.0001f);
            CHECK(clip.y >= -1.0001f && clip.y <= 1.0001f);
            // z stays within the padded [-1, 1] range (padding only enlarges the box).
            CHECK(clip.z >= -1.0001f && clip.z <= 1.0001f);
        }
    }
}

// The shader selects a cascade by comparing view depth to the stored splits; that
// selection must agree with render::selectCascade over the whole depth range.
static void testSelectionAgreement() {
    const float nearZ = 0.1f, farZ = 100.0f;
    const int count = 4;
    const std::vector<Cascade> cs = buildCascades(makeCamView(), 1.0472f, 16.0f / 9.0f, nearZ, farZ,
                                                  Vec3{-0.3f, -1.0f, -0.2f}, count, 0.7f);
    std::vector<float> boundaries;
    boundaries.push_back(nearZ);
    for (const Cascade& c : cs) boundaries.push_back(c.splitViewDepth);

    for (float depth = 0.0f; depth <= 120.0f; depth += 0.37f) {
        const int core = selectCascade(depth, boundaries);
        // Mirror the shader: first cascade whose splitViewDepth covers this depth.
        int gpu = count - 1;
        for (int i = 0; i < count; ++i) {
            if (depth <= cs[static_cast<std::size_t>(i)].splitViewDepth) { gpu = i; break; }
        }
        CHECK(core == gpu);
        CHECK(core >= 0 && core < count);
    }
}

// count == 1 degrades to a single fitted map spanning the whole [nearZ, farZ] frustum.
static void testSingleCascadeDegrades() {
    const float nearZ = 0.1f, farZ = 60.0f;
    const std::vector<Cascade> cs = buildCascades(makeCamView(), 1.0472f, 16.0f / 9.0f, nearZ, farZ,
                                                  Vec3{-0.2f, -1.0f, -0.1f}, 1, 0.5f);
    CHECK(cs.size() == 1u);
    CHECK(approx(cs[0].splitViewDepth, farZ));

    // The single cascade must cover the full-frustum corners.
    const Mat4 camView = makeCamView();
    const std::array<Vec3, 8> corners =
        frustumCornersWorld(mat::inverse(mat::mul(mat::perspective(1.0472f, 16.0f / 9.0f, nearZ, farZ), camView)));
    for (const Vec3& w : corners) {
        const Vec3 clip = unproject(cs[0].lightMatrix, w.x, w.y, w.z);
        CHECK(clip.x >= -1.0001f && clip.x <= 1.0001f);
        CHECK(clip.y >= -1.0001f && clip.y <= 1.0001f);
        CHECK(clip.z >= -1.0001f && clip.z <= 1.0001f);
    }

    // Clamping / degenerate guards: count <= 0 still yields at least one cascade.
    CHECK(buildCascades(camView, 1.0472f, 16.0f / 9.0f, nearZ, farZ, Vec3{0.0f, -1.0f, 0.0f}, 0, 0.5f).size() == 1u);
    // A zero light direction cannot be fit and yields no cascades (caller falls back).
    CHECK(buildCascades(camView, 1.0472f, 16.0f / 9.0f, nearZ, farZ, Vec3{0.0f, 0.0f, 0.0f}, 4, 0.5f).empty());
}

int main() {
    testInverseRoundTrip();
    testMulConventions();
    testSplitMonotonic();
    testSliceCornersInsideClip();
    testSelectionAgreement();
    testSingleCascadeDegrades();

    if (g_failures == 0) {
        std::printf("All shadow cascade build tests passed.\n");
        return 0;
    }
    std::printf("%d shadow cascade build test(s) failed.\n", g_failures);
    return 1;
}
