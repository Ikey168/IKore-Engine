#pragma once

#include <cmath>

/**
 * @file PbrBrdf.h
 * @brief Header-only Cook-Torrance PBR BRDF math (issue #233, rendering P2).
 *
 * First half of P2 in docs/RENDERING_MODERNIZATION.md: the metallic-roughness
 * microfacet math, isolated as pure functions so it is unit-testable without a GL
 * context (mirrors how RenderPassGraph.h shipped). No renderer changes here - this
 * is the math the future PBR shader path will mirror.
 *
 * Provides the three Cook-Torrance terms - the GGX/Trowbridge-Reitz normal
 * distribution D, the Smith geometry/visibility term G (Schlick-GGX with a
 * roughness remap), and the Fresnel-Schlick term F - plus sRGB<->linear
 * conversions. All scalar float math, std only.
 */
namespace IKore {
namespace render {

inline constexpr float kPi = 3.14159265358979323846f;

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/// Clamp perceptual roughness to (0, 1]. A small positive floor avoids the
/// division-by-zero mirror singularity at roughness 0 while staying finite.
inline float clampRoughness(float roughness) {
    return roughness < 1.0e-3f ? 1.0e-3f : (roughness > 1.0f ? 1.0f : roughness);
}

/// GGX alpha = roughness^2 (the "roughness remap" for the distribution).
inline float roughnessToAlpha(float roughness) {
    const float r = clampRoughness(roughness);
    return r * r;
}

/**
 * @brief GGX / Trowbridge-Reitz normal distribution D.
 * @param NdotH cos of the angle between the surface normal and the half vector.
 * @param roughness perceptual roughness in [0, 1].
 *
 * Normalized so that the integral of D * (N.H) over the hemisphere is 1.
 */
inline float distributionGGX(float NdotH, float roughness) {
    const float a = roughnessToAlpha(roughness);
    const float a2 = a * a;
    const float nh = clamp01(NdotH);
    const float nh2 = nh * nh;
    // Algebraically NdotH^2 * (a2 - 1) + 1, but written to avoid catastrophic
    // cancellation so a tiny a2 (very smooth surfaces) stays finite at NdotH = 1.
    const float d = nh2 * a2 + (1.0f - nh2);
    return a2 / (kPi * d * d);
}

/// Schlick-GGX geometry term for one direction, given the remapped roughness @p k.
inline float geometrySchlickGGX(float NdotX, float k) {
    const float x = clamp01(NdotX);
    const float denom = x * (1.0f - k) + k;
    return denom > 0.0f ? x / denom : 0.0f;
}

/// Roughness remap for direct (analytic) lighting: k = (roughness + 1)^2 / 8.
inline float roughnessToKDirect(float roughness) {
    const float r = clamp01(roughness);
    return (r + 1.0f) * (r + 1.0f) / 8.0f;
}

/// Roughness remap for image-based lighting: k = roughness^2 / 2.
inline float roughnessToKIBL(float roughness) {
    const float r = clamp01(roughness);
    return (r * r) / 2.0f;
}

/**
 * @brief Smith geometry term G = G1(NdotV) * G1(NdotL) using the direct-lighting
 *        roughness remap. Returns a masking/shadowing factor in [0, 1].
 */
inline float geometrySmith(float NdotV, float NdotL, float roughness) {
    const float k = roughnessToKDirect(roughness);
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

/**
 * @brief Fresnel-Schlick approximation F.
 * @param cosTheta cos of the angle between the view (or light) and the half vector.
 * @param f0 reflectance at normal incidence.
 *
 * Returns f0 at normal incidence and 1 at grazing angles (never below f0 or above 1
 * for f0 in [0, 1], so it does not create energy).
 */
inline float fresnelSchlick(float cosTheta, float f0) {
    const float m = 1.0f - clamp01(cosTheta);
    const float m5 = m * m * m * m * m; // (1 - cosTheta)^5
    return f0 + (1.0f - f0) * m5;
}

/// Normal-incidence reflectance F0 from an index of refraction (air interface).
/// For example n = 1.5 (glass) gives F0 = 0.04, the common dielectric default.
inline float fresnelF0FromIor(float ior) {
    const float r = (ior - 1.0f) / (ior + 1.0f);
    return r * r;
}

/// Convert an sRGB-encoded channel value to linear.
inline float srgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/// Convert a linear channel value to sRGB-encoded.
inline float linearToSrgb(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

} // namespace render
} // namespace IKore
