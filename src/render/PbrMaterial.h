#pragma once

#include "core/ecs/components/Components.h" // ecs::Vec3
#include "render/PbrBrdf.h"

#include <cmath>

/**
 * @file PbrMaterial.h
 * @brief Opt-in PBR material params + a reference direct-lighting evaluation (#234).
 *
 * Second half of rendering P2 (docs/RENDERING_MODERNIZATION.md): the material-side
 * data for a metallic-roughness path, plus a headless reference implementation of
 * the Cook-Torrance direct-lighting equation built on the tested PbrBrdf.h terms.
 *
 * The GLSL fragment shader (src/shaders/pbr.frag) mirrors evaluateDirectionalPbr()
 * exactly, so the shading math is unit-tested in C++ even though GLSL is not
 * compiled in CI. PbrMaterial defaults to disabled, so a material only takes the
 * PBR path when it explicitly opts in - the Phong path is left untouched.
 *
 * Header-only and GL-free (uses ecs::Vec3), so it unit-tests without a GL context.
 */
namespace IKore {
namespace render {

/// Metallic-roughness material inputs. `enabled` is the per-material opt-in flag.
struct PbrMaterial {
    bool enabled{false};
    ecs::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float ao{1.0f};
};

namespace detail {

inline float dot3(const ecs::Vec3& a, const ecs::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline ecs::Vec3 normalize3(const ecs::Vec3& v) {
    const float len = std::sqrt(dot3(v, v));
    return len > 0.0f ? ecs::Vec3{v.x / len, v.y / len, v.z / len} : ecs::Vec3{};
}

inline float maxf(float a, float b) { return a > b ? a : b; }

} // namespace detail

/**
 * @brief Cook-Torrance direct lighting from a single light, in linear space.
 * @param normal   Surface normal (world space; will be normalized).
 * @param viewDir  Direction from the surface to the camera (will be normalized).
 * @param lightDir Direction from the surface to the light (will be normalized).
 * @param radiance Incoming light color/intensity (already attenuated for point lights).
 * @param material Metallic-roughness inputs.
 * @return Outgoing radiance (unclamped). Zero when the light is below the horizon.
 *
 * This is the reference the GLSL PBR shader mirrors: diffuse comes only from the
 * non-metallic, non-Fresnel-reflected fraction, so the result is energy-aware.
 */
inline ecs::Vec3 evaluateDirectionalPbr(const ecs::Vec3& normal, const ecs::Vec3& viewDir,
                                        const ecs::Vec3& lightDir, const ecs::Vec3& radiance,
                                        const PbrMaterial& material) {
    using detail::dot3;
    using detail::maxf;
    using detail::normalize3;

    const ecs::Vec3 N = normalize3(normal);
    const ecs::Vec3 V = normalize3(viewDir);
    const ecs::Vec3 L = normalize3(lightDir);
    const ecs::Vec3 H = normalize3(ecs::Vec3{V.x + L.x, V.y + L.y, V.z + L.z});

    const float NdotL = maxf(dot3(N, L), 0.0f);
    if (NdotL <= 0.0f) return ecs::Vec3{}; // light below the horizon contributes nothing

    const float NdotV = maxf(dot3(N, V), 0.0f);
    const float NdotH = maxf(dot3(N, H), 0.0f);
    const float VdotH = maxf(dot3(V, H), 0.0f);

    // Base reflectance: 0.04 for dielectrics, tinted toward albedo for metals.
    const ecs::Vec3 f0{0.04f + (material.albedo.x - 0.04f) * material.metallic,
                       0.04f + (material.albedo.y - 0.04f) * material.metallic,
                       0.04f + (material.albedo.z - 0.04f) * material.metallic};

    const float d = distributionGGX(NdotH, material.roughness);
    const float g = geometrySmith(NdotV, NdotL, material.roughness);
    const ecs::Vec3 fresnel{fresnelSchlick(VdotH, f0.x), fresnelSchlick(VdotH, f0.y),
                            fresnelSchlick(VdotH, f0.z)};

    const float denom = 4.0f * NdotV * NdotL + 1.0e-4f;
    const ecs::Vec3 specular{d * g * fresnel.x / denom, d * g * fresnel.y / denom,
                             d * g * fresnel.z / denom};

    // Diffuse only from the fraction not reflected and not metallic.
    const float invPi = 1.0f / kPi;
    const ecs::Vec3 kd{(1.0f - fresnel.x) * (1.0f - material.metallic),
                       (1.0f - fresnel.y) * (1.0f - material.metallic),
                       (1.0f - fresnel.z) * (1.0f - material.metallic)};
    const ecs::Vec3 diffuse{kd.x * material.albedo.x * invPi, kd.y * material.albedo.y * invPi,
                            kd.z * material.albedo.z * invPi};

    return ecs::Vec3{(diffuse.x + specular.x) * radiance.x * NdotL,
                     (diffuse.y + specular.y) * radiance.y * NdotL,
                     (diffuse.z + specular.z) * radiance.z * NdotL};
}

} // namespace render
} // namespace IKore
