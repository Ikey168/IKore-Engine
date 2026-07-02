#pragma once

#include "core/Picking.h"          // pick::Mat4, pick::unproject, ecs::Vec3
#include "render/ShadowCascades.h" // cascadeSplitDistances, frustumCornersWorld, lightSpaceBounds, orthoFromBounds

#include <array>
#include <cmath>
#include <vector>

/**
 * @file ShadowCascadeBuild.h
 * @brief Per-cascade light-matrix assembly for the GPU shadow path (issue #265).
 *
 * ShadowCascades.h (#236) is the split/fit math: where to cut the view frustum and
 * how to fit an orthographic box around one slice. What it does not do is assemble a
 * ready-to-upload light matrix per cascade, because that needs the 4x4 matrix
 * algebra (perspective, lookAt, multiply, inverse) that the engine normally gets
 * from glm. This header supplies those operations on the glm-free, column-major
 * pick::Mat4 so the whole build is unit-testable without glm or a GL context, then
 * ties them together:
 *
 *   - mat::mul / perspective / lookAt / inverse: minimal column-major matrix algebra
 *     matching glm's conventions (so glm::value_ptr output and these agree bit-for-
 *     bit up to float rounding, and the results upload without transposition).
 *   - buildCascades(): for each cascade, narrow the camera projection to that depth
 *     slice, unproject its frustum corners to world space, fit a light-space box
 *     (padded so occluders behind the slice still cast into it), and return the
 *     per-cascade light matrix plus the view-space split depth the shader compares
 *     against. Cascade selection uses render::selectCascade, so CPU and GPU agree.
 *
 * count == 1 degrades to a single fitted map covering [nearZ, farZ] - the same shape
 * the pre-CSM directional map had - so the single-cascade path stays available.
 * Header-only, std only.
 */
namespace IKore {
namespace render {
namespace mat {

/// Column-major 4x4 multiply: result = a * b (glm convention, element (r,c) = m[c*4+r]).
inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            }
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

/// Right-handed perspective projection, OpenGL NDC z in [-1, 1] (matches glm::perspective).
inline Mat4 perspective(float fovyRad, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovyRad * 0.5f);
    const float safeAspect = std::fabs(aspect) > 1e-8f ? aspect : 1.0f;
    Mat4 r{}; // zero-initialized
    r.m[0] = f / safeAspect;
    r.m[5] = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

/// Right-handed look-at view matrix (matches glm::lookAt).
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = (center - eye).normalized();
    Vec3 s = Vec3{f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x};
    s = s.normalized();
    const Vec3 u = Vec3{s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x};

    Mat4 r = Mat4::identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    r.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    r.m[14] = (f.x * eye.x + f.y * eye.y + f.z * eye.z);
    return r;
}

/**
 * @brief General 4x4 inverse (cofactor method). Returns identity if singular.
 *
 * Column-major in and out, so inverse(proj * view) here equals
 * glm::inverse(proj * view) up to float rounding.
 */
inline Mat4 inverse(const Mat4& in) {
    const float* m = in.m;
    Mat4 out{};
    float* inv = out.m;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-20f) return Mat4::identity();
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) inv[i] *= det;
    return out;
}

} // namespace mat

/// One cascade ready for the GPU: its light matrix and the view-depth split it owns.
struct Cascade {
    Mat4 lightMatrix{};       ///< ortho(light-space fit) * lightView; world -> cascade clip.
    float splitViewDepth{0};  ///< far boundary of this cascade in positive view depth.
};

/**
 * @brief Build the per-cascade light matrices and split depths for a directional light.
 * @param camView   Camera view matrix (world -> view), column-major.
 * @param fovyRad   Camera vertical field of view in radians.
 * @param aspect    Camera aspect ratio (width / height).
 * @param nearZ     Camera near-plane distance (> 0).
 * @param farZ      Camera far-plane distance (> nearZ).
 * @param lightDir  Direction the light travels (from light toward scene); need not be unit.
 * @param count     Number of cascades (clamped to >= 1). count == 1 = single fitted map.
 * @param lambda    Split blend passed to cascadeSplitDistances (0 uniform .. 1 log).
 * @param zPadding  Extra light-space depth pulled back behind each slice so occluders
 *                  outside the view frustum still cast shadows into it.
 * @return count cascades, near-slice first. Empty only if inputs are degenerate.
 *
 * Cascade i covers view depth [splits[i], splits[i+1]] and its splitViewDepth is the
 * far end splits[i+1] - the same value render::selectCascade compares a fragment's
 * view depth against, so the shader picks the identical cascade the CPU fitted.
 */
inline std::vector<Cascade> buildCascades(const Mat4& camView, float fovyRad, float aspect,
                                          float nearZ, float farZ, const Vec3& lightDir,
                                          int count, float lambda, float zPadding = 10.0f) {
    if (count < 1) count = 1;
    std::vector<Cascade> cascades;
    const Vec3 dir = lightDir.normalized();
    if (dir.lengthSquared() < 1e-12f) return cascades; // no light direction to fit to

    const std::vector<float> splits = cascadeSplitDistances(nearZ, farZ, count, lambda);

    // A stable up vector: world up unless the light is (near) vertical, then use +Z.
    const Vec3 worldUp = std::fabs(dir.y) > 0.999f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};

    cascades.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float sliceNear = splits[static_cast<std::size_t>(i)];
        const float sliceFar = splits[static_cast<std::size_t>(i) + 1u];

        // World-space corners of just this depth slice of the camera frustum.
        const Mat4 sliceProj = mat::perspective(fovyRad, aspect, sliceNear, sliceFar);
        const Mat4 sliceViewProj = mat::mul(sliceProj, camView);
        const std::array<Vec3, 8> corners = frustumCornersWorld(mat::inverse(sliceViewProj));

        // Frustum centroid to anchor the light "camera".
        Vec3 center{0.0f, 0.0f, 0.0f};
        for (const Vec3& c : corners) center += c;
        center *= (1.0f / 8.0f);

        // Look at the slice from along the light direction; ortho fit handles distance.
        const Mat4 lightView = mat::lookAt(center - dir, center, worldUp);

        Aabb box = lightSpaceBounds(corners, lightView);
        box.min.z -= zPadding; // pull the near plane back so off-frustum occluders cast in
        box.max.z += zPadding;

        Cascade cascade;
        cascade.lightMatrix = mat::mul(orthoFromBounds(box), lightView);
        cascade.splitViewDepth = sliceFar;
        cascades.push_back(cascade);
    }
    return cascades;
}

} // namespace render
} // namespace IKore
