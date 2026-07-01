#pragma once

#include <cstddef>
#include <vector>

/**
 * @file ShadowFiltering.h
 * @brief Percentage-closer filtering (PCF) math for soft shadows
 *        (issue #237, rendering modernization P4).
 *
 * A raw shadow map yields a single depth comparison per fragment, so edges are
 * hard and alias badly. PCF averages the comparison over several taps around the
 * shadow lookup, turning the binary edge into a soft gradient. This header is the
 * renderer-agnostic, dependency-free, unit-testable core behind that:
 *
 *   - shadowTexelSize(): the texel step for a square shadow map.
 *   - shadowTest(): one depth comparison (in-shadow vs lit), matching the shader.
 *   - pcfBoxOffsets(): a square (2h+1)x(2h+1) tap grid for a kernel size.
 *   - poissonDiskOffsets(): a 16-tap Poisson disk, which trades the regular grid's
 *     banding for noise that reads as smoother at the same tap count.
 *   - pcfFilter(): average shadowTest over a tap set, scaled by texel size and a
 *     softness factor. Dividing by the actual tap count keeps even kernels correct
 *     (the older inline shader divided by kernelSize*kernelSize).
 *
 * The lighting shader's ShadowCalculation mirrors this the way pbr.frag mirrors
 * PbrBrdf.h: the same tap sets, the same per-tap comparison, the same average, so
 * the soft-shadow look can be reasoned about and tested headlessly even though
 * GLSL is not compiled in CI and the GL engine cannot run here. A kernel size of 1
 * (single origin tap) reproduces the hard-shadow path exactly, so that path stays
 * available. Header-only, std only.
 */
namespace IKore {
namespace render {

/// Texel size (step between adjacent texels in UV) for a square shadow map.
inline float shadowTexelSize(int resolution) {
    return resolution > 0 ? 1.0f / static_cast<float>(resolution) : 0.0f;
}

/// One shadow comparison: 1.0 if the fragment is occluded, 0.0 if lit. Mirrors the
/// shader's `currentDepth - bias > closestDepth`. Bias offsets the fragment depth
/// toward the light to combat shadow acne.
inline float shadowTest(float currentDepth, float closestDepth, float bias) {
    return (currentDepth - bias > closestDepth) ? 1.0f : 0.0f;
}

/// A 2D sampling offset in texel units (before scaling by texel size / softness).
struct Offset2D {
    float x;
    float y;
};

/**
 * @brief Square PCF tap grid for a kernel size.
 * @param kernelSize Filter width in texels (clamped to >= 1).
 * @return (2h+1)^2 offsets, h = kernelSize/2, spanning [-h, h] on each axis.
 *
 * kernelSize 1 yields a single (0, 0) tap, i.e. the hard-shadow sample. Odd sizes
 * (3, 5, 7) give the usual centered kernels; the count is always the true number
 * of taps so pcfFilter's average is correct for any size.
 */
inline std::vector<Offset2D> pcfBoxOffsets(int kernelSize) {
    if (kernelSize < 1) kernelSize = 1;
    const int half = kernelSize / 2;
    const int side = 2 * half + 1;
    std::vector<Offset2D> offsets;
    offsets.reserve(static_cast<std::size_t>(side * side));
    for (int x = -half; x <= half; ++x) {
        for (int y = -half; y <= half; ++y) {
            offsets.push_back(Offset2D{static_cast<float>(x), static_cast<float>(y)});
        }
    }
    return offsets;
}

/**
 * @brief A fixed 16-tap Poisson disk (the widely used shadow-mapping set).
 * @return 16 offsets distributed within a small disk around the origin.
 *
 * Sampling a Poisson disk instead of a regular grid replaces the grid's structured
 * banding with unstructured noise, which the eye reads as a smoother penumbra at
 * the same tap count. The shader uses the identical constants so CPU and GPU agree.
 */
inline std::vector<Offset2D> poissonDiskOffsets() {
    return {
        {-0.94201624f, -0.39906216f}, {0.94558609f, -0.76890725f},
        {-0.09418410f, -0.92938870f}, {0.34495938f, 0.29387760f},
        {-0.91588581f, 0.45771432f},  {-0.81544232f, -0.87912464f},
        {-0.38277543f, 0.27676845f},  {0.97484398f, 0.75648379f},
        {0.44323325f, -0.97511554f},  {0.53742981f, -0.47373420f},
        {-0.26496911f, -0.41893023f}, {0.79197514f, 0.19090188f},
        {-0.24188840f, 0.99706507f},  {-0.81409955f, 0.91437590f},
        {0.19984126f, 0.78641367f},   {0.14383161f, -0.14100790f},
    };
}

/**
 * @brief Average the shadow test over a tap set (percentage-closer filtering).
 * @param u,v          Shadow-map UV of the fragment.
 * @param currentDepth Fragment depth in light space.
 * @param bias         Depth bias against acne.
 * @param texelSize    Shadow-map texel size (see shadowTexelSize()).
 * @param softness     Multiplier on the tap footprint (1.0 = one texel per unit
 *                     offset; larger widens the penumbra).
 * @param offsets      Tap offsets in texel units (box grid or Poisson disk).
 * @param sampleDepth  Callable (float su, float sv) -> stored shadow-map depth.
 * @return Shadow fraction in [0, 1]; 0 fully lit, 1 fully occluded.
 *
 * Each tap samples the shadow map at (u, v) + offset * texelSize * softness and
 * contributes one shadowTest; the result is the mean over all taps. With a
 * single-tap kernel this equals a plain shadowTest, so the hard path is preserved.
 */
template <class SampleFn>
inline float pcfFilter(float u, float v, float currentDepth, float bias,
                       float texelSize, float softness,
                       const std::vector<Offset2D>& offsets, SampleFn sampleDepth) {
    if (offsets.empty()) return 0.0f;
    float shadow = 0.0f;
    for (const Offset2D& o : offsets) {
        const float su = u + o.x * texelSize * softness;
        const float sv = v + o.y * texelSize * softness;
        shadow += shadowTest(currentDepth, sampleDepth(su, sv), bias);
    }
    return shadow / static_cast<float>(offsets.size());
}

} // namespace render
} // namespace IKore
