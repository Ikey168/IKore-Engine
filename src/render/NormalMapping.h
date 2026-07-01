#pragma once

#include "core/ecs/components/Components.h" // for ecs::Vec3

#include <cmath>

/**
 * @file NormalMapping.h
 * @brief Tangent-space normal mapping math (issue #238, rendering modernization P4).
 *
 * A normal map stores a per-texel surface normal in tangent space (the RGB encoding
 * of a unit normal). To light with it, each fragment needs a tangent basis (TBN:
 * tangent, bitangent, normal) that maps tangent space to world space; the sampled
 * normal is unpacked and rotated by that basis. This header is the renderer-agnostic,
 * glm-free, unit-testable core behind that:
 *
 *   - computeTangentBasis(): per-triangle tangent/bitangent from positions and UVs
 *     (the "compute on load if absent" fallback; Assimp does this at import time).
 *   - orthonormalizeTangent(): Gram-Schmidt the tangent against the vertex normal,
 *     exactly as the vertex shader does before building the TBN.
 *   - computeBitangent(): the right-handed bitangent cross(N, T) the shader uses.
 *   - bitangentSign(): handedness (+1 / -1) for meshes with mirrored UVs.
 *   - unpackNormal(): decode an RGB texel in [0, 1] to a normal in [-1, 1].
 *   - transformNormalToWorld(): rotate a tangent-space normal by the TBN basis.
 *
 * The Phong vertex/fragment shaders mirror orthonormalizeTangent / computeBitangent /
 * unpackNormal / transformNormalToWorld line for line, the way pbr.frag mirrors
 * PbrBrdf.h, so the perturbed-normal math can be tested headlessly even though GLSL
 * is not compiled in CI and the GL engine cannot run here. A flat normal texel
 * (0.5, 0.5, 1.0) unpacks to (0, 0, 1) and transforms back to the geometric normal,
 * so an absent or flat normal map leaves shading unchanged. Header-only, std only.
 */
namespace IKore {
namespace render {

using Vec3 = ecs::Vec3;

/// A 2D texture coordinate.
struct Vec2 {
    float x;
    float y;
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// A per-triangle tangent basis. @c valid is false when the UVs are degenerate.
struct TangentBasis {
    Vec3 tangent;
    Vec3 bitangent;
    bool valid;
};

/**
 * @brief Compute a triangle's tangent and bitangent from positions and UVs.
 *
 * Solves the standard system relating the two triangle edges to their UV deltas:
 *   tangent   = f * ( dV2 * edge1 - dV1 * edge2 )
 *   bitangent = f * ( dU1 * edge2 - dU2 * edge1 ),  f = 1 / (dU1*dV2 - dU2*dV1)
 * so the tangent follows +U and the bitangent +V across the surface. When the UV
 * area is zero (degenerate mapping) the determinant vanishes; the result is marked
 * invalid and a safe default basis is returned instead of dividing by zero.
 */
inline TangentBasis computeTangentBasis(const Vec3& p0, const Vec3& p1, const Vec3& p2,
                                        const Vec2& uv0, const Vec2& uv1, const Vec2& uv2) {
    const Vec3 edge1 = p1 - p0;
    const Vec3 edge2 = p2 - p0;
    const float du1 = uv1.x - uv0.x;
    const float dv1 = uv1.y - uv0.y;
    const float du2 = uv2.x - uv0.x;
    const float dv2 = uv2.y - uv0.y;

    const float det = du1 * dv2 - du2 * dv1;
    if (std::fabs(det) < 1e-12f) {
        return TangentBasis{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, false};
    }
    const float f = 1.0f / det;
    const Vec3 tangent = (edge1 * dv2 - edge2 * dv1) * f;
    const Vec3 bitangent = (edge2 * du1 - edge1 * du2) * f;
    return TangentBasis{tangent, bitangent, true};
}

/**
 * @brief Gram-Schmidt orthonormalize a tangent against a normal.
 *
 * Removes the component of @p tangent along @p normal and normalizes, giving a unit
 * tangent perpendicular to the (interpolated) normal. Mirrors the vertex shader's
 * `T = normalize(T - dot(T, N) * N)`.
 */
inline Vec3 orthonormalizeTangent(const Vec3& normal, const Vec3& tangent) {
    const Vec3 n = normal.normalized();
    const Vec3 t = tangent - n * dot(n, tangent);
    return t.normalized();
}

/// Right-handed bitangent cross(N, T), matching the vertex shader's TBN construction.
inline Vec3 computeBitangent(const Vec3& normal, const Vec3& tangent) {
    return cross(normal, tangent);
}

/**
 * @brief Handedness of a tangent basis: +1 right-handed, -1 mirrored.
 *
 * For meshes with mirrored UVs the stored bitangent points opposite cross(N, T);
 * this sign (usually kept in tangent.w) lets the shader flip it back.
 */
inline float bitangentSign(const Vec3& normal, const Vec3& tangent, const Vec3& bitangent) {
    return dot(cross(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
}

/// Decode a normal-map RGB sample in [0, 1] to a tangent-space normal in [-1, 1].
/// Mirrors the shader's `texture(...).rgb * 2.0 - 1.0`.
inline Vec3 unpackNormal(const Vec3& rgb) {
    return Vec3{2.0f * rgb.x - 1.0f, 2.0f * rgb.y - 1.0f, 2.0f * rgb.z - 1.0f};
}

/**
 * @brief Rotate a tangent-space normal into world space by the TBN basis.
 *
 * Computes normalize(T * n.x + B * n.y + N * n.z), i.e. the TBN matrix times the
 * tangent-space normal, matching the fragment shader's `normalize(TBN * n)`. A flat
 * normal (0, 0, 1) maps to the geometric normal N, so a flat map is a no-op.
 */
inline Vec3 transformNormalToWorld(const Vec3& tangent, const Vec3& bitangent,
                                   const Vec3& normal, const Vec3& tangentNormal) {
    const Vec3 world = tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z;
    return world.normalized();
}

} // namespace render
} // namespace IKore
