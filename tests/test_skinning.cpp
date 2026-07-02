// Test for linear-blend skinning + root-motion math - issue #260.
//
// src/shaders/skinned_phong.vert mirrors skinPosition/skinDirection (same guard
// and fallback semantics), Model.cpp mirrors normalizeInfluenceWeights when
// importing bone weights, and AnimationComponent::updateRootMotion applies the
// rootMotionTranslationDelta(+Looped) math - so this suite verifies the skinning
// pipeline's correctness-critical math headlessly.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_skinning.cpp -o test_skinning

#include "render/Skinning.h"

#include <cmath>
#include <cstdio>

using namespace IKore::render::skin;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }
static bool approxVec(const Vec3& a, const Vec3& b, float eps = 1e-5f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

static Mat4 translation(float x, float y, float z) {
    Mat4 m = Mat4::identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

// 90-degree rotation about +Z, column-major.
static Mat4 rotationZ90() {
    Mat4 m = Mat4::identity();
    m.m[0] = 0.0f; m.m[1] = 1.0f;
    m.m[4] = -1.0f; m.m[5] = 0.0f;
    return m;
}

static void testTransformHelpers() {
    const Mat4 t = translation(1, 2, 3);
    CHECK(approxVec(transformPoint(t, Vec3{1, 1, 1}), Vec3{2, 3, 4}));
    // Directions ignore translation.
    CHECK(approxVec(transformDirection(t, Vec3{1, 1, 1}), Vec3{1, 1, 1}));

    const Mat4 r = rotationZ90();
    CHECK(approxVec(transformPoint(r, Vec3{1, 0, 0}), Vec3{0, 1, 0}));
    CHECK(approxVec(transformDirection(r, Vec3{0, 1, 0}), Vec3{-1, 0, 0}));
}

static void testNormalizeWeights() {
    float w1[4] = {2.0f, 2.0f, 0.0f, 0.0f};
    normalizeInfluenceWeights(w1);
    CHECK(approx(w1[0], 0.5f) && approx(w1[1], 0.5f) && approx(w1[2], 0.0f));

    // Negative garbage is zeroed before normalization.
    float w2[4] = {3.0f, -1.0f, 1.0f, 0.0f};
    normalizeInfluenceWeights(w2);
    CHECK(approx(w2[0], 0.75f) && approx(w2[1], 0.0f) && approx(w2[2], 0.25f));

    // No influences at all -> rigid bind to slot 0.
    float w3[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    normalizeInfluenceWeights(w3);
    CHECK(approx(w3[0], 1.0f) && approx(w3[1], 0.0f));
}

static void testSkinPosition() {
    const Mat4 palette[2] = {translation(1, 0, 0), translation(0, 2, 0)};
    const Vec3 p{0, 0, 0};

    // Single full-weight bone.
    {
        const int ids[4] = {0, -1, -1, -1};
        const float w[4] = {1, 0, 0, 0};
        CHECK(approxVec(skinPosition(palette, 2, ids, w, p), Vec3{1, 0, 0}));
    }
    // 50/50 blend of two translations.
    {
        const int ids[4] = {0, 1, -1, -1};
        const float w[4] = {0.5f, 0.5f, 0, 0};
        CHECK(approxVec(skinPosition(palette, 2, ids, w, p), Vec3{0.5f, 1.0f, 0.0f}));
    }
    // Out-of-range bone ids are skipped; the valid one still applies fully.
    {
        const int ids[4] = {7, 0, -1, -1};
        const float w[4] = {0.5f, 0.5f, 0, 0};
        CHECK(approxVec(skinPosition(palette, 2, ids, w, p), Vec3{0.5f, 0, 0}));
    }
    // No applicable influence -> rigid fallback (vertex unchanged).
    {
        const int ids[4] = {-1, -1, -1, -1};
        const float w[4] = {0, 0, 0, 0};
        CHECK(approxVec(skinPosition(palette, 2, ids, w, Vec3{3, 4, 5}), Vec3{3, 4, 5}));
    }
}

static void testSkinDirection() {
    const Mat4 palette[2] = {rotationZ90(), translation(9, 9, 9)};
    const int ids[4] = {0, -1, -1, -1};
    const float w[4] = {1, 0, 0, 0};
    // Rotated, unit length.
    CHECK(approxVec(skinDirection(palette, 2, ids, w, Vec3{1, 0, 0}), Vec3{0, 1, 0}));

    // A pure-translation bone leaves a direction unchanged (w = 0 transform).
    const int ids2[4] = {1, -1, -1, -1};
    CHECK(approxVec(skinDirection(palette, 2, ids2, w, Vec3{0, 0, 1}), Vec3{0, 0, 1}));

    // Blended directions come back normalized.
    const int ids3[4] = {0, 1, -1, -1};
    const float w3[4] = {0.5f, 0.5f, 0, 0};
    const Vec3 blended = skinDirection(palette, 2, ids3, w3, Vec3{1, 0, 0});
    CHECK(approx(blended.length(), 1.0f, 1e-4f));

    // No influences -> the original direction, normalized.
    const int none[4] = {-1, -1, -1, -1};
    CHECK(approxVec(skinDirection(palette, 2, none, w, Vec3{0, 3, 0}), Vec3{0, 1, 0}));
}

static void testRootMotionDelta() {
    // Within one loop iteration: plain difference.
    CHECK(approxVec(rootMotionTranslationDelta(Vec3{1, 0, 0}, Vec3{1.5f, 0, 0}),
                    Vec3{0.5f, 0, 0}));

    // Across a wrap: remainder to the clip end plus advance from the clip start.
    // Clip moves from x=0 to x=2; previous sample was at 1.8, current (wrapped) 0.3.
    const Vec3 delta = rootMotionTranslationDeltaLooped(
        Vec3{1.8f, 0, 0}, Vec3{0.3f, 0, 0}, Vec3{0, 0, 0}, Vec3{2, 0, 0});
    CHECK(approxVec(delta, Vec3{0.5f, 0, 0})); // (2-1.8) + (0.3-0)

    // A stationary root yields zero delta both ways.
    CHECK(approxVec(rootMotionTranslationDelta(Vec3{4, 5, 6}, Vec3{4, 5, 6}), Vec3{0, 0, 0}));
    CHECK(approxVec(rootMotionTranslationDeltaLooped(Vec3{1, 1, 1}, Vec3{1, 1, 1},
                                                     Vec3{1, 1, 1}, Vec3{1, 1, 1}),
                    Vec3{0, 0, 0}));

    // Sum over a whole walking loop equals the clip's total displacement:
    // samples at 0.0 -> 0.7 -> 1.4 -> (wrap) 0.1 with clip [0, 2] along x.
    Vec3 total{0, 0, 0};
    total += rootMotionTranslationDelta(Vec3{0, 0, 0}, Vec3{0.7f, 0, 0});
    total += rootMotionTranslationDelta(Vec3{0.7f, 0, 0}, Vec3{1.4f, 0, 0});
    total += rootMotionTranslationDeltaLooped(Vec3{1.4f, 0, 0}, Vec3{0.1f, 0, 0},
                                              Vec3{0, 0, 0}, Vec3{2, 0, 0});
    CHECK(approxVec(total, Vec3{2.1f, 0, 0})); // one full clip (2) + 0.1 into the next
}

int main() {
    testTransformHelpers();
    testNormalizeWeights();
    testSkinPosition();
    testSkinDirection();
    testRootMotionDelta();

    if (g_failures == 0) {
        std::printf("All skinning tests passed.\n");
        return 0;
    }
    std::printf("%d skinning test(s) failed.\n", g_failures);
    return 1;
}
