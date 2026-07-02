#pragma once

#include "render/DebugFont8x8.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file DebugTextGeometry.h
 * @brief Font-atlas and quad-geometry math for the on-screen debug text renderer
 *        (issue #259, renderer P-completions).
 *
 * EntityDebugSystem lays its overlay out in normalized screen coordinates
 * (top-left origin, x right, y down, both in [0, 1]). This header turns strings
 * into everything the GL side needs, as pure testable math:
 *
 *   - buildFontAtlas(): a 128x48 single-channel image holding the 96 printable
 *     ASCII glyphs in a 16x6 grid of 8x8 cells (255 = glyph pixel, 0 = empty).
 *     Row 0 of the returned buffer is texel row 0, i.e. the v=0 edge of the GL
 *     texture, so the UVs below match a plain glTexImage2D upload.
 *   - buildTextQuads(): two triangles (6 vertices) per printable character, with
 *     positions in the overlay's normalized space and UVs into the atlas.
 *     Handles newlines; characters outside the table render as '?'.
 *   - buildSolidQuad(): one untextured quad (for the overlay background).
 *   - atlasPixelAt(): nearest-texel sampling with GL's v convention, so tests can
 *     verify that quads + atlas together reproduce the font bits end to end.
 *
 * The GL side (EntityDebugSystem) mirrors this by uploading the atlas verbatim
 * and drawing the vertex stream; all layout decisions live (and are tested) here.
 */
namespace IKore {
namespace render {

inline constexpr int kDebugAtlasCols = 16;
inline constexpr int kDebugAtlasRows = 6;
inline constexpr int kDebugGlyphSize = 8;
inline constexpr int kDebugAtlasWidth = kDebugAtlasCols * kDebugGlyphSize;  // 128
inline constexpr int kDebugAtlasHeight = kDebugAtlasRows * kDebugGlyphSize; // 48

/// One vertex of the debug-text stream: normalized screen position + atlas UV.
struct TextVertex {
    float x;
    float y;
    float u;
    float v;
};

/// Table index for a character; unknown characters map to '?'.
inline int debugGlyphIndex(char c) {
    const int code = static_cast<unsigned char>(c);
    if (code < kDebugFontFirstChar || code >= kDebugFontFirstChar + kDebugFontGlyphCount) {
        return '?' - kDebugFontFirstChar;
    }
    return code - kDebugFontFirstChar;
}

/// True if the glyph pixel at column @p px, row @p py (top-left origin) is set.
inline bool debugGlyphPixel(int glyphIndex, int px, int py) {
    if (glyphIndex < 0 || glyphIndex >= kDebugFontGlyphCount) return false;
    if (px < 0 || px >= kDebugGlyphSize || py < 0 || py >= kDebugGlyphSize) return false;
    // Row byte py; bit px, bit 0 = leftmost pixel (font8x8 convention).
    return (kDebugFont8x8[glyphIndex][py] >> px) & 1;
}

/**
 * @brief Rasterize the whole font into a single-channel atlas (row 0 first, which
 *        GL treats as the v=0 texel row). Glyph i sits in cell (i%16, i/16); within
 *        a cell, the glyph's TOP row is stored at the LOWER v so that the UVs from
 *        buildTextQuads() (v grows downward on screen) sample it upright.
 */
inline std::vector<std::uint8_t> buildFontAtlas() {
    std::vector<std::uint8_t> atlas(static_cast<std::size_t>(kDebugAtlasWidth) * kDebugAtlasHeight, 0);
    for (int g = 0; g < kDebugFontGlyphCount; ++g) {
        const int cellX = (g % kDebugAtlasCols) * kDebugGlyphSize;
        const int cellY = (g / kDebugAtlasCols) * kDebugGlyphSize;
        for (int py = 0; py < kDebugGlyphSize; ++py) {
            for (int px = 0; px < kDebugGlyphSize; ++px) {
                if (!debugGlyphPixel(g, px, py)) continue;
                const int ax = cellX + px;
                const int ay = cellY + py;
                atlas[static_cast<std::size_t>(ay) * kDebugAtlasWidth + ax] = 255;
            }
        }
    }
    return atlas;
}

/// Nearest-texel atlas sample using GL's convention (v = 0 is texel row 0, i.e.
/// the first row of the buffer returned by buildFontAtlas()).
inline std::uint8_t atlasPixelAt(const std::vector<std::uint8_t>& atlas, float u, float v) {
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    int x = static_cast<int>(u * kDebugAtlasWidth);
    int y = static_cast<int>(v * kDebugAtlasHeight);
    if (x >= kDebugAtlasWidth) x = kDebugAtlasWidth - 1;
    if (y >= kDebugAtlasHeight) y = kDebugAtlasHeight - 1;
    return atlas[static_cast<std::size_t>(y) * kDebugAtlasWidth + x];
}

/// Append one axis-aligned quad as two CCW triangles (6 vertices).
inline void appendQuad(std::vector<TextVertex>& out, float x0, float y0, float x1, float y1,
                       float u0, float v0, float u1, float v1) {
    out.push_back(TextVertex{x0, y0, u0, v0});
    out.push_back(TextVertex{x0, y1, u0, v1});
    out.push_back(TextVertex{x1, y1, u1, v1});
    out.push_back(TextVertex{x0, y0, u0, v0});
    out.push_back(TextVertex{x1, y1, u1, v1});
    out.push_back(TextVertex{x1, y0, u1, v0});
}

/// One untextured quad (UVs zero) for the overlay background.
inline std::vector<TextVertex> buildSolidQuad(float x, float y, float width, float height) {
    std::vector<TextVertex> out;
    appendQuad(out, x, y, x + width, y + height, 0.0f, 0.0f, 0.0f, 0.0f);
    return out;
}

/**
 * @brief Build the vertex stream for @p text starting at (@p x, @p y) (the glyph
 *        cell's top-left), one @p charW x @p charH cell per character, y growing
 *        downward. '\n' returns to @p x and advances one line. Returns the number
 *        of printable glyph quads emitted (vertices = 6 * that).
 */
inline std::size_t buildTextQuads(const std::string& text, float x, float y,
                                  float charW, float charH, std::vector<TextVertex>& out) {
    std::size_t quads = 0;
    float penX = x;
    float penY = y;
    for (char c : text) {
        if (c == '\n') {
            penX = x;
            penY += charH;
            continue;
        }
        const int g = debugGlyphIndex(c);
        const float u0 = static_cast<float>((g % kDebugAtlasCols) * kDebugGlyphSize) / kDebugAtlasWidth;
        const float v0 = static_cast<float>((g / kDebugAtlasCols) * kDebugGlyphSize) / kDebugAtlasHeight;
        const float u1 = u0 + static_cast<float>(kDebugGlyphSize) / kDebugAtlasWidth;
        const float v1 = v0 + static_cast<float>(kDebugGlyphSize) / kDebugAtlasHeight;
        appendQuad(out, penX, penY, penX + charW, penY + charH, u0, v0, u1, v1);
        penX += charW;
        ++quads;
    }
    return quads;
}

} // namespace render
} // namespace IKore
