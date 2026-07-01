#pragma once

#include "core/ecs/components/Components.h" // ecs::Vec3
#include "render/PbrBrdf.h"                  // clamp01

#include <cmath>

/**
 * @file ToneMapping.h
 * @brief HDR exposure + ACES tone mapping math (issue #235, rendering P3).
 *
 * The resolve step of the HDR pipeline: take a linear HDR color, apply an exposure
 * multiplier, map it into displayable [0, 1] with the Narkowicz ACES filmic curve,
 * and gamma-encode (pow 1/2.2, matching the engine's existing bloom_combine.frag).
 * Kept as pure functions so the tone curve is unit-testable without a GL context;
 * the resolve fragment shader (src/shaders/tonemap.frag) mirrors these exactly.
 *
 * Header-only, GL-free (uses ecs::Vec3 for the color overload).
 */
namespace IKore {
namespace render {

/**
 * @brief Narkowicz ACES filmic tone-map curve for one channel.
 * @param x Linear HDR value (negative inputs are clamped to 0).
 * @return Tone-mapped value in [0, 1]. Saturates toward 1, so bright highlights
 *         roll off instead of clipping/overflowing.
 */
inline float acesFilmic(float x) {
    if (x < 0.0f) x = 0.0f;
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}

/// Apply exposure to a linear HDR channel (simple linear scale).
inline float applyExposure(float linearHdr, float exposure) { return linearHdr * exposure; }

/**
 * @brief Full resolve for one channel: exposure -> ACES -> sRGB encode.
 * @return Display value in [0, 1].
 */
inline float resolveChannel(float linearHdr, float exposure) {
    return std::pow(acesFilmic(applyExposure(linearHdr, exposure)), 1.0f / 2.2f);
}

/// Resolve a linear HDR color to a display color (per channel).
inline ecs::Vec3 resolveColor(const ecs::Vec3& linearHdr, float exposure) {
    return ecs::Vec3{resolveChannel(linearHdr.x, exposure), resolveChannel(linearHdr.y, exposure),
                     resolveChannel(linearHdr.z, exposure)};
}

} // namespace render
} // namespace IKore
