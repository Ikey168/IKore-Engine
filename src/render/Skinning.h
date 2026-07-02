#pragma once

#include "core/Picking.h" // pick::Mat4 (column-major), ecs::Vec3

#include <cmath>

/**
 * @file Skinning.h
 * @brief Linear-blend skinning and root-motion delta math (issue #260).
 *
 * The headless, unit-testable core behind GPU skinning and root motion:
 *
 *   - skinPosition / skinDirection: classic linear-blend skinning - a vertex is
 *     transformed by each influencing bone's palette matrix and the results are
 *     blended by the bone weights. src/shaders/skinned_phong.vert mirrors this
 *     line for line (positions with w = 1, directions with w = 0), including the
 *     out-of-range-bone guard and the no-influence identity fallback.
 *   - normalizeInfluenceWeights: loader-side cleanup so per-vertex weights sum to
 *     one (Model.cpp mirrors it when importing bone weights).
 *   - rootMotionTranslationDelta(+Looped): the per-frame root displacement
 *     AnimationComponent::updateRootMotion extracts from the root bone's track,
 *     including the loop-wrap step (end-of-clip remainder plus start-of-clip
 *     advance).
 *
 * Matrices are column-major pick::Mat4 (glm-layout compatible), vectors the
 * glm-free ecs::Vec3, so everything here runs without GL or glm. Header-only.
 */
namespace IKore {
namespace render {

namespace skin {
using Vec3 = ecs::Vec3;
using Mat4 = pick::Mat4;

/// Affine transform of a point (w = 1) by a column-major matrix.
inline Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return Vec3{m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
                m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
                m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}

/// Transform of a direction (w = 0): rotation/scale only, no translation.
inline Vec3 transformDirection(const Mat4& m, const Vec3& d) {
    return Vec3{m.m[0] * d.x + m.m[4] * d.y + m.m[8] * d.z,
                m.m[1] * d.x + m.m[5] * d.y + m.m[9] * d.z,
                m.m[2] * d.x + m.m[6] * d.y + m.m[10] * d.z};
}

inline constexpr int kMaxInfluences = 4;

/**
 * @brief Normalize a vertex's 4 influence weights in place so they sum to 1.
 *        A vertex with no positive weight gets full weight on its first slot,
 *        so an unskinned vertex still binds rigidly instead of collapsing.
 */
inline void normalizeInfluenceWeights(float weights[kMaxInfluences]) {
    float sum = 0.0f;
    for (int i = 0; i < kMaxInfluences; ++i) {
        if (weights[i] > 0.0f) sum += weights[i];
        else weights[i] = 0.0f;
    }
    if (sum <= 0.0f) {
        weights[0] = 1.0f;
        return;
    }
    for (int i = 0; i < kMaxInfluences; ++i) weights[i] /= sum;
}

/**
 * @brief Linear-blend skin a position: sum of weight * (palette[bone] * position)
 *        over the vertex's influences. Bones outside [0, paletteCount) are
 *        skipped (the shader guards identically); if no influence applies, the
 *        vertex stays rigid (identity), matching the shader's fallback.
 */
inline Vec3 skinPosition(const Mat4* palette, int paletteCount,
                         const int boneIds[kMaxInfluences],
                         const float weights[kMaxInfluences], const Vec3& position) {
    Vec3 out{0.0f, 0.0f, 0.0f};
    float applied = 0.0f;
    for (int i = 0; i < kMaxInfluences; ++i) {
        const int id = boneIds[i];
        if (id < 0 || id >= paletteCount || weights[i] <= 0.0f) continue;
        out += transformPoint(palette[id], position) * weights[i];
        applied += weights[i];
    }
    if (applied <= 0.0f) return position; // rigid fallback
    return out;
}

/// Linear-blend skin a direction (normal/tangent); normalized like the shader.
inline Vec3 skinDirection(const Mat4* palette, int paletteCount,
                          const int boneIds[kMaxInfluences],
                          const float weights[kMaxInfluences], const Vec3& direction) {
    Vec3 out{0.0f, 0.0f, 0.0f};
    float applied = 0.0f;
    for (int i = 0; i < kMaxInfluences; ++i) {
        const int id = boneIds[i];
        if (id < 0 || id >= paletteCount || weights[i] <= 0.0f) continue;
        out += transformDirection(palette[id], direction) * weights[i];
        applied += weights[i];
    }
    if (applied <= 0.0f) return direction.normalized();
    return out.normalized();
}

/**
 * @brief Root displacement between two samples of the root bone's translation
 *        track within the same loop iteration.
 */
inline Vec3 rootMotionTranslationDelta(const Vec3& previous, const Vec3& current) {
    return current - previous;
}

/**
 * @brief Root displacement across a loop wrap: the remainder of the clip after
 *        the previous sample (end - previous) plus the advance into the new
 *        iteration (current - start). With this, a walking loop keeps moving
 *        forward smoothly instead of snapping back on wrap.
 */
inline Vec3 rootMotionTranslationDeltaLooped(const Vec3& previous, const Vec3& current,
                                             const Vec3& clipStart, const Vec3& clipEnd) {
    return (clipEnd - previous) + (current - clipStart);
}

} // namespace skin
} // namespace render
} // namespace IKore
