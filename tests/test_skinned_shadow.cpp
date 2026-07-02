// Test for skinned shadow depth passes - issue #266.
//
// skinned_shadow_depth.vert / skinned_shadow_point.vert apply linear-blend skinning
// so a skinned mesh's shadow matches its animated pose. They must skin a position
// exactly as skinned_phong.vert does - i.e. exactly as the tested render::skin core -
// or the shadow would drift from the lit geometry. GLSL is not compiled in CI and the
// GL engine cannot run here, so this transcribes the shaders' skinning loop (the same
// out-of-range guard and no-influence fallback) into C++ and asserts it agrees with
// render::skin::skinPosition, then checks the shadow-pass selection/upload policy.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_skinned_shadow.cpp -o test_skinned_shadow

#include "render/ShadowSkinning.h"
#include "render/Skinning.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;
using IKore::pick::Mat4;
using Vec3 = IKore::ecs::Vec3;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

// A column-major translation matrix (glm/pick layout), like a bone's palette entry.
static Mat4 translation(float x, float y, float z) {
    Mat4 m = Mat4::identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

// Exact transcription of the skinning loop in skinned_shadow_depth.vert (position
// only): guard out-of-range/zero-weight bones, blend by weight, rigid fallback when
// nothing applies. If this ever diverges from render::skin::skinPosition, the shadow
// pose diverges from the lit pose - which is the bug #266 fixes.
static Vec3 skinLikeShadowShader(const std::vector<Mat4>& palette,
                                 const int boneIds[4], const float weights[4], const Vec3& pos) {
    const int maxBones = 100; // MAX_BONES in the shader
    Vec3 skinned{0.0f, 0.0f, 0.0f};
    float applied = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const int id = boneIds[i];
        if (id < 0 || id >= maxBones || weights[i] <= 0.0f) continue;
        skinned += skin::transformPoint(palette[static_cast<std::size_t>(id)], pos) * weights[i];
        applied += weights[i];
    }
    if (applied <= 0.0f) return pos; // rigid fallback
    return skinned;
}

static void testShadowMatchesCore() {
    // Model the palette exactly as the shader sees it: the finalBonesMatrices uniform
    // is always MAX_BONES entries, identity-padded for unused slots. Passing the same
    // count to the core makes the shared guard boundary (id >= MAX_BONES) line up, so
    // the comparison isolates the skinning math rather than a guard mismatch.
    std::vector<Mat4> palette(static_cast<std::size_t>(kShadowMaxBones), Mat4::identity());
    palette[0] = translation(0.0f, 0.0f, 0.0f);
    palette[1] = translation(2.0f, 0.0f, 0.0f);
    palette[2] = translation(0.0f, 3.0f, 0.0f);
    palette[3] = translation(-1.0f, 0.0f, 4.0f);
    const Vec3 pos{1.0f, 1.0f, 1.0f};

    struct Case {
        int ids[4];
        float w[4];
    } cases[] = {
        {{1, 2, -1, -1}, {0.5f, 0.5f, 0.0f, 0.0f}},   // two-bone blend
        {{0, 3, 1, 2}, {0.25f, 0.25f, 0.25f, 0.25f}}, // four-bone blend
        {{5, 2, -1, -1}, {0.5f, 0.5f, 0.0f, 0.0f}},   // identity-padded bone + real bone
        {{2, -1, 3, -1}, {0.6f, 0.0f, 0.4f, 0.0f}},   // negative weight slots skipped
        {{200, -1, -1, -1}, {1.0f, 0.0f, 0.0f, 0.0f}},// id >= MAX_BONES -> fallback
        {{-1, -1, -1, -1}, {0.0f, 0.0f, 0.0f, 0.0f}}, // no influence -> rigid
        {{1, -1, -1, -1}, {1.0f, 0.0f, 0.0f, 0.0f}},  // single full-weight bone
    };

    for (const Case& c : cases) {
        const Vec3 core = skin::skinPosition(palette.data(), kShadowMaxBones, c.ids, c.w, pos);
        const Vec3 shader = skinLikeShadowShader(palette, c.ids, c.w, pos);
        CHECK(approx(core, shader));
    }
}

static void testProgramSelection() {
    CHECK(shadowProgramFor(true) == ShadowProgram::Skinned);
    CHECK(shadowProgramFor(false) == ShadowProgram::Static);
}

static void testPaletteUploadClamp() {
    CHECK(shadowPaletteUploadCount(0) == 0);
    CHECK(shadowPaletteUploadCount(37) == 37);
    CHECK(shadowPaletteUploadCount(kShadowMaxBones) == kShadowMaxBones);
    CHECK(shadowPaletteUploadCount(kShadowMaxBones + 50) == kShadowMaxBones); // clamp down
    CHECK(shadowPaletteUploadCount(-5) == 0);                                 // negative -> 0
    CHECK(shadowPaletteUploadCount(9, 8) == 8);                               // custom cap
}

int main() {
    testShadowMatchesCore();
    testProgramSelection();
    testPaletteUploadClamp();

    if (g_failures == 0) {
        std::printf("All skinned shadow tests passed.\n");
        return 0;
    }
    std::printf("%d skinned shadow test(s) failed.\n", g_failures);
    return 1;
}
