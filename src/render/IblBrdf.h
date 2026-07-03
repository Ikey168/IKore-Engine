#pragma once

#include "render/PbrBrdf.h" // clamp01, kPi, roughnessToAlpha, roughnessToKIBL, geometrySchlickGGX

#include <cmath>
#include <cstdint>

/**
 * @file IblBrdf.h
 * @brief Split-sum image-based-lighting math for PBR (issue #270), header-only.
 *
 * Direct PBR lighting shipped with PbrBrdf.h (#233); IBL completes the look with a
 * diffuse irradiance map, prefiltered specular environment mips, and the split-sum
 * BRDF integration LUT. The GPU generates those textures by convolving the skybox
 * cubemap, but the sampling/weighting math is pure and unit-testable, so it lives
 * here as the reference the generation shaders mirror (the way pbr.frag mirrors
 * PbrBrdf.h):
 *
 *   - hammersley() / radicalInverseVdC(): the low-discrepancy sequence the importance
 *     sampling draws from.
 *   - importanceSampleGGX(): a GGX-distributed half-vector in tangent space (N = +Z),
 *     used by both the prefilter pass and the BRDF LUT.
 *   - integrateBrdfLut(): the split-sum environment BRDF (scale, bias) that brdf_lut
 *     .frag mirrors, built on PbrBrdf.h's Smith geometry with the IBL roughness remap.
 *   - mipToRoughness() / roughnessToMip(): the prefilter mip <-> roughness mapping the
 *     specular sample uses (textureLod level = roughness * maxMip).
 *
 * All std-only float math; no GL, so it unit-tests without a context.
 */
namespace IKore {
namespace render {

/// A minimal 2D/3D vector for the sampling math (kept local so this stays GL-free).
struct Vec2f {
    float x{0.0f};
    float y{0.0f};
};
struct Vec3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

/// Van der Corput radical inverse of @p bits in base 2, in [0, 1).
inline float radicalInverseVdC(std::uint32_t bits) {
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 2^32
}

/// The i-th of @p n points of the Hammersley sequence: (i/n, radicalInverse(i)).
inline Vec2f hammersley(std::uint32_t i, std::uint32_t n) {
    const float invN = n > 0u ? 1.0f / static_cast<float>(n) : 0.0f;
    return Vec2f{static_cast<float>(i) * invN, radicalInverseVdC(i)};
}

/**
 * @brief A GGX-importance-sampled half-vector in tangent space (surface normal = +Z).
 * @param xi        A point from hammersley() in [0, 1)^2.
 * @param roughness Perceptual roughness in [0, 1].
 * @return Unit half-vector; roughness 0 concentrates it at +Z (mirror), higher
 *         roughness spreads it toward the hemisphere.
 *
 * Because the tangent frame has N = +Z, the returned vector is also the world-space
 * half-vector when the shader builds its basis around N. brdf_lut.frag and
 * prefilter.frag mirror this exactly.
 */
inline Vec3f importanceSampleGGX(const Vec2f& xi, float roughness) {
    const float a = roughnessToAlpha(roughness); // alpha = roughness^2
    const float phi = 2.0f * kPi * xi.x;
    const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
    return Vec3f{std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta};
}

/// Smith geometry for IBL: G1(NdotV) * G1(NdotL) with the IBL remap k = roughness^2/2.
inline float geometrySmithIBL(float NdotV, float NdotL, float roughness) {
    const float k = roughnessToKIBL(roughness);
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

/**
 * @brief Split-sum environment BRDF integration: the (scale, bias) LUT entry.
 * @param NdotV       cos angle between normal and view, clamped to (0, 1].
 * @param roughness   Perceptual roughness in [0, 1].
 * @param sampleCount Number of importance samples (>= 1).
 * @return Vec2f{scale, bias}: specular reflectance = F0 * scale + bias.
 *
 * This is the environment half of the split-sum approximation (Karis 2013), the exact
 * value brdf_lut.frag writes into the LUT texture. It depends only on NdotV and
 * roughness (not on the environment), so precomputing it once is valid for any scene.
 */
inline Vec2f integrateBrdfLut(float NdotV, float roughness, std::uint32_t sampleCount = 1024u) {
    NdotV = NdotV < 1.0e-4f ? 1.0e-4f : (NdotV > 1.0f ? 1.0f : NdotV);
    // View in tangent space (N = +Z), in the plane y = 0.
    const Vec3f V{std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV};

    float A = 0.0f;
    float B = 0.0f;
    const std::uint32_t n = sampleCount > 0u ? sampleCount : 1u;
    for (std::uint32_t i = 0u; i < n; ++i) {
        const Vec2f xi = hammersley(i, n);
        const Vec3f H = importanceSampleGGX(xi, roughness);
        const float VdotH = V.x * H.x + V.y * H.y + V.z * H.z;
        // L = reflect(-V, H) = 2 (V.H) H - V; only its z (= NdotL) is needed.
        const Vec3f L{2.0f * VdotH * H.x - V.x, 2.0f * VdotH * H.y - V.y, 2.0f * VdotH * H.z - V.z};
        const float NdotL = L.z > 0.0f ? L.z : 0.0f;
        const float NdotH = H.z > 0.0f ? H.z : 0.0f;
        const float vh = VdotH > 0.0f ? VdotH : 0.0f;
        if (NdotL > 0.0f) {
            const float g = geometrySmithIBL(NdotV, NdotL, roughness);
            const float gVis = (g * vh) / (NdotH * NdotV + 1.0e-8f);
            const float fc = std::pow(1.0f - vh, 5.0f);
            A += (1.0f - fc) * gVis;
            B += fc * gVis;
        }
    }
    return Vec2f{A / static_cast<float>(n), B / static_cast<float>(n)};
}

/// Prefilter mip level -> perceptual roughness: roughness = mip / maxMipLevel.
inline float mipToRoughness(int mip, int maxMipLevel) {
    if (maxMipLevel <= 0) return 0.0f;
    return clamp01(static_cast<float>(mip) / static_cast<float>(maxMipLevel));
}

/// Perceptual roughness -> prefilter LOD: level = roughness * maxMipLevel (the value
/// the shader passes to textureLod on the prefiltered environment map).
inline float roughnessToMip(float roughness, int maxMipLevel) {
    return clamp01(roughness) * static_cast<float>(maxMipLevel > 0 ? maxMipLevel : 0);
}

} // namespace render
} // namespace IKore
