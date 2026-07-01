#pragma once

/**
 * @file UiScaling.h
 * @brief DPI / resolution aware UI scaling math (issue #61).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless core behind DPI- and
 * resolution-aware UI: combine a user-facing scale multiplier, the monitor DPI /
 * content scale, and an optional resolution auto-scale (relative to the resolution
 * the UI is authored at) into a single effective scale, clamped to a readable range.
 * Plus aspect-ratio classification so the layout can adapt across aspect ratios.
 *
 * The engine applies the returned scale cheaply (ImGui FontGlobalScale +
 * ScaleAllSizes and the HUD scale), so there is no font-atlas rebuild and the cost
 * is negligible. Pure math, no ImGui - unit-testable on its own. Header-only.
 */
namespace IKore {

/// Broad aspect-ratio buckets the layout can adapt to.
enum class AspectCategory { Tall, Standard, Wide, UltraWide };

/// Configuration for the effective UI scale. userScale is the user-facing setting.
struct UiScaleConfig {
    float referenceHeight{1080.0f}; ///< Resolution height the UI is authored at.
    float userScale{1.0f};          ///< User-facing multiplier.
    float minScale{0.5f};
    float maxScale{3.0f};
    bool autoResolutionScale{true}; ///< Scale with framebuffer height vs referenceHeight.
};

/**
 * @brief Effective UI scale for the given framebuffer height and DPI.
 * @param cfg               Scale configuration (user setting, reference, clamps).
 * @param framebufferHeight Current framebuffer/logical height in the same units as
 *                          cfg.referenceHeight.
 * @param dpiScale          Monitor DPI / content scale (1.0 at 96 DPI). Values <= 0
 *                          are treated as 1.0.
 * @return userScale * dpiScale * (auto-resolution factor), clamped to [min, max].
 */
inline float computeUiScale(const UiScaleConfig& cfg, float framebufferHeight, float dpiScale) {
    float scale = cfg.userScale * (dpiScale > 0.0f ? dpiScale : 1.0f);
    if (cfg.autoResolutionScale && cfg.referenceHeight > 0.0f && framebufferHeight > 0.0f) {
        scale *= framebufferHeight / cfg.referenceHeight;
    }
    if (scale < cfg.minScale) scale = cfg.minScale;
    if (scale > cfg.maxScale) scale = cfg.maxScale;
    return scale;
}

/// Aspect ratio (width / height); 0 if height is non-positive.
inline float aspectRatio(float width, float height) { return height > 0.0f ? width / height : 0.0f; }

/// Classify an aspect ratio into a broad bucket for adaptive layout.
inline AspectCategory classifyAspect(float width, float height) {
    const float r = aspectRatio(width, height);
    if (r < 1.0f) return AspectCategory::Tall;      // portrait
    if (r < 1.55f) return AspectCategory::Standard; // 4:3, 3:2
    if (r < 2.0f) return AspectCategory::Wide;      // 16:10, 16:9
    return AspectCategory::UltraWide;               // 21:9 and wider
}

inline const char* aspectCategoryName(AspectCategory category) {
    switch (category) {
        case AspectCategory::Tall: return "Tall";
        case AspectCategory::Standard: return "Standard";
        case AspectCategory::Wide: return "Wide";
        case AspectCategory::UltraWide: return "UltraWide";
    }
    return "Unknown";
}

} // namespace IKore
