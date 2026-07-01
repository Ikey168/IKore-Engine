#pragma once

#include "core/Picking.h" // for pick::Mat4, pick::unproject, ecs::Vec3

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

/**
 * @file ShadowCascades.h
 * @brief Cascaded Shadow Maps (CSM) math foundation for the directional light
 *        (issue #236, rendering modernization P4).
 *
 * A single directional shadow map has to cover the whole view frustum at one
 * resolution, so near-field shadows look blocky while far-field detail is wasted.
 * CSM splits the view frustum into a few depth ranges (cascades) and gives each
 * its own tightly fitted shadow map, so texels concentrate where the camera is
 * looking. This header is the renderer-agnostic, glm-free, unit-testable math
 * behind that:
 *
 *   - cascadeSplitDistances(): where to cut the frustum along view depth, using
 *     the practical (logarithmic/uniform blend) split scheme.
 *   - selectCascade(): which cascade a given view-space depth falls into. Mirrors
 *     the per-fragment selection the shader does, so CPU and GPU agree.
 *   - frustumCornersWorld(): the eight world-space corners of a (sub-)frustum,
 *     unprojected from the NDC cube through an inverse view-projection.
 *   - lightSpaceBounds() + orthoFromBounds(): fit a tight light-space AABB around
 *     a cascade's corners and build the orthographic projection that maps it to
 *     the [-1, 1] clip cube, i.e. the per-cascade light matrix.
 *
 * Matrices are column-major (matching glm and pick::Mat4), so the engine can pass
 * glm::value_ptr(...) straight in, and the ortho this builds can be uploaded as a
 * light-space matrix without transposition. Vectors reuse the glm-free ecs::Vec3,
 * so the whole thing is testable without glm or a GL context. Header-only.
 *
 * The GPU wiring (a depth texture array, one shadow pass per cascade, and the
 * shader sampling the selected cascade with a cross-cascade blend band) mirrors
 * this core the way pbr.frag mirrors PbrBrdf.h and tonemap.frag mirrors
 * ToneMapping.h; that integration is a visual follow-up since it cannot be
 * verified headlessly.
 */
namespace IKore {
namespace render {

using Vec3 = ecs::Vec3;
using Mat4 = pick::Mat4;

/**
 * @brief Split view-space depth into cascade boundaries using the practical scheme.
 * @param nearZ  Camera near-plane distance (view space, > 0).
 * @param farZ   Camera far-plane distance (view space, > nearZ).
 * @param count  Number of cascades (clamped to >= 1).
 * @param lambda Blend between uniform (0) and logarithmic (1) splits, clamped [0, 1].
 * @return count+1 boundary distances [nearZ, s1, ..., farZ], strictly increasing.
 *
 * Cascade i covers the depth range [result[i], result[i+1]]. The uniform scheme
 * spaces splits evenly in depth (good far-field coverage); the logarithmic scheme
 * packs them toward the camera (good near-field detail). The practical scheme
 * (Zhang et al., "Parallel-Split Shadow Maps") blends the two:
 *   split(i) = lambda * near * (far/near)^(i/count) + (1 - lambda) * (near + (far-near)*i/count)
 * The endpoints are pinned exactly to nearZ and farZ so callers can rely on them.
 */
inline std::vector<float> cascadeSplitDistances(float nearZ, float farZ, int count, float lambda) {
    if (count < 1) count = 1;
    if (lambda < 0.0f) lambda = 0.0f;
    if (lambda > 1.0f) lambda = 1.0f;
    // The logarithmic term needs a strictly positive near distance.
    const float safeNear = nearZ > 1e-6f ? nearZ : 1e-6f;

    std::vector<float> splits(static_cast<std::size_t>(count) + 1u);
    splits.front() = nearZ;
    splits.back() = farZ;
    const float range = farZ - safeNear;
    const float ratio = farZ / safeNear;
    for (int i = 1; i < count; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(count);
        const float logSplit = safeNear * std::pow(ratio, p);
        const float uniformSplit = safeNear + range * p;
        splits[static_cast<std::size_t>(i)] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
    }
    return splits;
}

/**
 * @brief Pick the cascade index for a view-space depth, mirroring the shader.
 * @param viewDepth  Positive distance from the camera along view Z.
 * @param boundaries The count+1 boundaries from cascadeSplitDistances().
 * @return Cascade index in [0, count-1].
 *
 * Cascade i owns depths up to boundaries[i+1]. Depths at or beyond the far
 * boundary clamp to the last cascade, and depths at or before the near boundary
 * fall into cascade 0, so every depth maps to a valid cascade. The engine and the
 * shader must share this selection or fragments sample the wrong cascade at the seams.
 */
inline int selectCascade(float viewDepth, const std::vector<float>& boundaries) {
    if (boundaries.size() < 2) return 0;
    const int count = static_cast<int>(boundaries.size()) - 1;
    for (int i = 0; i < count; ++i) {
        if (viewDepth <= boundaries[static_cast<std::size_t>(i) + 1u]) return i;
    }
    return count - 1;
}

/**
 * @brief The eight world-space corners of a frustum given its inverse view-projection.
 * @param invViewProj Inverse of (projection * view) for the frustum (or sub-frustum).
 * @return Corners of the NDC cube unprojected to world space.
 *
 * Corners span the OpenGL NDC cube (x, y, z each in {-1, +1}); z = -1 is the near
 * plane and z = +1 the far plane. To fit a single cascade, pass the inverse
 * view-projection of a camera whose near/far were narrowed to that cascade's
 * depth slice.
 */
inline std::array<Vec3, 8> frustumCornersWorld(const Mat4& invViewProj) {
    std::array<Vec3, 8> corners{};
    int idx = 0;
    for (int xi = 0; xi < 2; ++xi) {
        const float ndcX = xi == 0 ? -1.0f : 1.0f;
        for (int yi = 0; yi < 2; ++yi) {
            const float ndcY = yi == 0 ? -1.0f : 1.0f;
            for (int zi = 0; zi < 2; ++zi) {
                const float ndcZ = zi == 0 ? -1.0f : 1.0f;
                corners[static_cast<std::size_t>(idx++)] = pick::unproject(invViewProj, ndcX, ndcY, ndcZ);
            }
        }
    }
    return corners;
}

/// An axis-aligned bounding box in some space (here, light space).
struct Aabb {
    Vec3 min;
    Vec3 max;
};

/**
 * @brief Fit an axis-aligned bounding box around frustum corners in light space.
 * @param corners   World-space frustum corners (from frustumCornersWorld()).
 * @param lightView The directional light's view matrix (world -> light space, affine).
 * @return The component-wise min/max of the corners transformed into light space.
 *
 * Transforming by an affine view matrix keeps w = 1, so the perspective divide in
 * unproject() is a no-op here; it is reused purely as a point transform. The
 * resulting box is what a cascade's orthographic projection should cover.
 */
inline Aabb lightSpaceBounds(const std::array<Vec3, 8>& corners, const Mat4& lightView) {
    const float big = 1e30f;
    Aabb box{Vec3{big, big, big}, Vec3{-big, -big, -big}};
    for (const Vec3& worldCorner : corners) {
        const Vec3 p = pick::unproject(lightView, worldCorner.x, worldCorner.y, worldCorner.z);
        box.min.x = std::min(box.min.x, p.x);
        box.min.y = std::min(box.min.y, p.y);
        box.min.z = std::min(box.min.z, p.z);
        box.max.x = std::max(box.max.x, p.x);
        box.max.y = std::max(box.max.y, p.y);
        box.max.z = std::max(box.max.z, p.z);
    }
    return box;
}

/**
 * @brief Build the orthographic projection that maps a light-space AABB to clip space.
 * @param box A light-space AABB (from lightSpaceBounds()).
 * @return A column-major ortho matrix mapping box.min -> -1 and box.max -> +1 on x, y, z.
 *
 * This is the per-cascade light matrix (combined with the light view when the
 * corners were transformed): a point at the box center lands at the clip origin,
 * min corners at (-1,-1,-1) and max corners at (+1,+1,+1). Degenerate extents
 * fall back to a unit span to avoid division by zero.
 */
inline Mat4 orthoFromBounds(const Aabb& box) {
    float rl = box.max.x - box.min.x;
    float tb = box.max.y - box.min.y;
    float fn = box.max.z - box.min.z;
    if (std::fabs(rl) < 1e-8f) rl = 1.0f;
    if (std::fabs(tb) < 1e-8f) tb = 1.0f;
    if (std::fabs(fn) < 1e-8f) fn = 1.0f;

    Mat4 r{};
    r.m[0] = 2.0f / rl;
    r.m[5] = 2.0f / tb;
    r.m[10] = 2.0f / fn;
    r.m[12] = -(box.max.x + box.min.x) / rl;
    r.m[13] = -(box.max.y + box.min.y) / tb;
    r.m[14] = -(box.max.z + box.min.z) / fn;
    r.m[15] = 1.0f;
    return r;
}

} // namespace render
} // namespace IKore
