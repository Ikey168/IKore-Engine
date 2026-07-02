#pragma once

#include "core/ecs/components/Components.h" // ecs::Vec3
#include "core/sim/Simulation.h"            // sim::DeterministicRng

#include <cmath>
#include <cstdint>
#include <vector>

/**
 * @file SsaoKernel.h
 * @brief SSAO sampling-kernel and occlusion math (issue #259, renderer P-completions).
 *
 * The headless, deterministic core behind the screen-space ambient-occlusion pass:
 *
 *   - generateSsaoKernel(): the hemisphere sample kernel (tangent-space vectors,
 *     z >= 0) with the classic accelerating scale ramp that clusters samples near
 *     the origin, seeded via DeterministicRng so the same seed yields the same
 *     kernel on every run/platform (the old generator used std::random_device).
 *   - generateSsaoNoise(): the 4x4 tangent-rotation vectors (z = 0) that decorrelate
 *     neighboring pixels' kernels.
 *   - ssaoNoiseScale(): the screen-to-noise tiling factor (screen / 4), replacing a
 *     1280x720 constant that was hardcoded in the shader.
 *   - smoothstep01 / ssaoRangeCheck / ssaoSampleOcclusion / ssaoResolve: line-for-
 *     line mirrors of the shader's per-sample occlusion test, range falloff, and
 *     final resolve, so the AO response is unit-testable headlessly.
 *
 * src/shaders/ssao.frag mirrors these the way pbr.frag mirrors PbrBrdf.h. Header-
 * only, std + the engine's glm-free value types.
 */
namespace IKore {
namespace render {

/// Scale ramp for sample @p i of @p count: lerp(0.1, 1.0, (i/count)^2). Clusters
/// early samples near the shaded point where occlusion matters most.
inline float ssaoScaleRamp(int i, int count) {
    if (count <= 0) return 0.1f;
    const float t = static_cast<float>(i) / static_cast<float>(count);
    return 0.1f + (t * t) * 0.9f;
}

/// Deterministic hemisphere sample kernel (unit-or-shorter vectors with z >= 0).
inline std::vector<ecs::Vec3> generateSsaoKernel(int count, std::uint64_t seed = 1337) {
    std::vector<ecs::Vec3> kernel;
    if (count <= 0) return kernel;
    kernel.reserve(static_cast<std::size_t>(count));
    sim::DeterministicRng rng(seed);
    for (int i = 0; i < count; ++i) {
        ecs::Vec3 sample{rng.nextFloat() * 2.0f - 1.0f,
                         rng.nextFloat() * 2.0f - 1.0f,
                         rng.nextFloat()}; // hemisphere: z in [0, 1)
        sample = sample.normalized();
        sample *= rng.nextFloat();        // vary length within the hemisphere
        sample *= ssaoScaleRamp(i, count);
        kernel.push_back(sample);
    }
    return kernel;
}

/// Deterministic 4x4 noise: tangent-rotation vectors in the XY plane (z = 0).
inline std::vector<ecs::Vec3> generateSsaoNoise(std::uint64_t seed = 4242) {
    std::vector<ecs::Vec3> noise;
    noise.reserve(16);
    sim::DeterministicRng rng(seed);
    for (int i = 0; i < 16; ++i) {
        noise.push_back(ecs::Vec3{rng.nextFloat() * 2.0f - 1.0f,
                                  rng.nextFloat() * 2.0f - 1.0f,
                                  0.0f});
    }
    return noise;
}

/// Screen-to-noise-texture tiling factors for a WxH framebuffer and the 4x4 noise.
inline void ssaoNoiseScale(int width, int height, float& scaleX, float& scaleY) {
    scaleX = static_cast<float>(width) / 4.0f;
    scaleY = static_cast<float>(height) / 4.0f;
}

/// GLSL-equivalent smoothstep(edge0, edge1, x).
inline float smoothstep01(float edge0, float edge1, float x) {
    const float denom = edge1 - edge0;
    float t = denom != 0.0f ? (x - edge0) / denom : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/// Range falloff: full weight when the potential occluder is within @p radius of
/// the shaded fragment along view Z, fading out beyond it (kills distant-geometry
/// false occlusion). Mirrors smoothstep(0, 1, radius / abs(fragZ - sampleViewZ)).
inline float ssaoRangeCheck(float radius, float fragZ, float sampleViewZ) {
    const float dz = std::fabs(fragZ - sampleViewZ);
    if (dz < 1e-9f) return 1.0f;
    return smoothstep01(0.0f, 1.0f, radius / dz);
}

/**
 * @brief One kernel sample's occlusion contribution, mirroring the shader:
 *        (sampleViewZ >= samplePosZ + bias ? 1 : 0) * rangeCheck.
 *
 * View space looks down -Z, so a LARGER (less negative) Z is CLOSER to the camera:
 * the sample point is occluded when the depth buffer's surface at that screen
 * position (@p sampleViewZ) is in front of the kernel sample point (@p samplePosZ).
 * @p bias suppresses self-occlusion acne on flat surfaces.
 */
inline float ssaoSampleOcclusion(float sampleViewZ, float samplePosZ, float bias,
                                 float radius, float fragZ) {
    const float occluded = sampleViewZ >= samplePosZ + bias ? 1.0f : 0.0f;
    return occluded * ssaoRangeCheck(radius, fragZ, sampleViewZ);
}

/// Final AO term: 1 fully lit, approaching 0 as more samples are occluded.
inline float ssaoResolve(float occlusionSum, int sampleCount) {
    if (sampleCount <= 0) return 1.0f;
    return 1.0f - occlusionSum / static_cast<float>(sampleCount);
}

/// The multiplier the combine pass applies to the scene color: mix(1, ao,
/// intensity) with intensity clamped to [0, 1], so intensity 0 leaves the image
/// untouched and 1 applies the full AO term. Mirrors ssao_combine.frag.
inline float ssaoCombineWeight(float ao, float intensity) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    return 1.0f + (ao - 1.0f) * intensity;
}

} // namespace render
} // namespace IKore
