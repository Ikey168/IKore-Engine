// Test for the debug-text atlas and quad geometry - issue #259 (renderer
// completions).
//
// EntityDebugSystem's GL renderer uploads buildFontAtlas() verbatim and draws
// buildTextQuads()'s vertex stream, so verifying here that the quads + atlas
// reproduce the font bits end to end (via GL's sampling convention) validates the
// on-screen result headlessly. Also covers glyph mapping, layout/advance/newline,
// and the background quad.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_debug_text.cpp -o test_debug_text

#include "render/DebugTextGeometry.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore::render;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

static void testGlyphIndexing() {
    CHECK(debugGlyphIndex(' ') == 0);
    CHECK(debugGlyphIndex('A') == 'A' - 32);
    CHECK(debugGlyphIndex('~') == '~' - 32);
    // Out-of-table characters render as '?'.
    CHECK(debugGlyphIndex(static_cast<char>(200)) == '?' - 32);
    CHECK(debugGlyphIndex('\x1f') == '?' - 32);
    // DEL (127) is the table's last, blank glyph - in range, renders as nothing.
    CHECK(debugGlyphIndex(static_cast<char>(127)) == 127 - 32);
}

static void testAtlasContents() {
    const std::vector<std::uint8_t> atlas = buildFontAtlas();
    CHECK(atlas.size() == static_cast<std::size_t>(kDebugAtlasWidth) * kDebugAtlasHeight);

    // The space glyph's cell is entirely empty; 'A' has a plausible pixel count.
    auto cellCount = [&](char c) {
        const int g = debugGlyphIndex(c);
        const int cx = (g % kDebugAtlasCols) * kDebugGlyphSize;
        const int cy = (g / kDebugAtlasCols) * kDebugGlyphSize;
        int n = 0;
        for (int y = 0; y < kDebugGlyphSize; ++y) {
            for (int x = 0; x < kDebugGlyphSize; ++x) {
                if (atlas[static_cast<std::size_t>(cy + y) * kDebugAtlasWidth + (cx + x)]) ++n;
            }
        }
        return n;
    };
    CHECK(cellCount(' ') == 0);
    CHECK(cellCount('A') > 10 && cellCount('A') < 40);
    CHECK(cellCount('.') > 0 && cellCount('.') < 10);

    // Spot-check a known bit: 'A' row 1 is 0x1E -> pixel (1,1) set, pixel (0,1) not.
    CHECK(debugGlyphPixel(debugGlyphIndex('A'), 1, 1));
    CHECK(!debugGlyphPixel(debugGlyphIndex('A'), 0, 1));
}

static void testQuadLayout() {
    std::vector<TextVertex> verts;
    const std::size_t quads = buildTextQuads("AB", 0.1f, 0.2f, 0.02f, 0.03f, verts);
    CHECK(quads == 2u);
    CHECK(verts.size() == 12u);
    // First quad at the pen origin; second advanced by one cell width.
    CHECK(approx(verts[0].x, 0.1f) && approx(verts[0].y, 0.2f));
    CHECK(approx(verts[6].x, 0.1f + 0.02f));
    // Quad corners span the cell.
    CHECK(approx(verts[2].x, 0.1f + 0.02f) && approx(verts[2].y, 0.2f + 0.03f));

    // Newline returns to the left margin and advances one line.
    verts.clear();
    CHECK(buildTextQuads("A\nB", 0.1f, 0.2f, 0.02f, 0.03f, verts) == 2u);
    CHECK(approx(verts[6].x, 0.1f));
    CHECK(approx(verts[6].y, 0.2f + 0.03f));

    // Empty text yields nothing.
    verts.clear();
    CHECK(buildTextQuads("", 0, 0, 0.02f, 0.03f, verts) == 0u);
    CHECK(verts.empty());
}

static void testBackgroundQuad() {
    const std::vector<TextVertex> q = buildSolidQuad(0.05f, 0.1f, 0.4f, 0.3f);
    CHECK(q.size() == 6u);
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const TextVertex& v : q) {
        minX = std::fmin(minX, v.x);
        maxX = std::fmax(maxX, v.x);
        minY = std::fmin(minY, v.y);
        maxY = std::fmax(maxY, v.y);
        CHECK(v.u == 0.0f && v.v == 0.0f); // untextured
    }
    CHECK(approx(minX, 0.05f) && approx(maxX, 0.45f));
    CHECK(approx(minY, 0.1f) && approx(maxY, 0.4f));
}

// The decisive check: rendering the quads with the atlas (using GL's sampling
// convention, exactly what the GL side does) reproduces the font bits pixel for
// pixel - so the text on screen is the font, upright and unmirrored.
static void testQuadsPlusAtlasReproduceFont() {
    const std::vector<std::uint8_t> atlas = buildFontAtlas();
    const std::string text = "IK9.";
    std::vector<TextVertex> verts;
    const float charW = 0.05f, charH = 0.08f, originX = 0.3f, originY = 0.4f;
    buildTextQuads(text, originX, originY, charW, charH, verts);
    CHECK(verts.size() == text.size() * 6u);

    for (std::size_t ci = 0; ci < text.size(); ++ci) {
        const int g = debugGlyphIndex(text[ci]);
        // The quad's corner vertices for this character (see appendQuad's order).
        const TextVertex& topLeft = verts[ci * 6 + 0];
        const TextVertex& bottomRight = verts[ci * 6 + 2];
        // Sample the quad at each glyph-pixel center: screen position (px+.5)/8
        // across the cell, interpolated UV likewise; compare with the font bit.
        for (int py = 0; py < kDebugGlyphSize; ++py) {
            for (int px = 0; px < kDebugGlyphSize; ++px) {
                const float fx = (px + 0.5f) / kDebugGlyphSize;
                const float fy = (py + 0.5f) / kDebugGlyphSize;
                const float u = topLeft.u + (bottomRight.u - topLeft.u) * fx;
                const float v = topLeft.v + (bottomRight.v - topLeft.v) * fy;
                const bool lit = atlasPixelAt(atlas, u, v) != 0;
                const bool expected = debugGlyphPixel(g, px, py);
                if (lit != expected) {
                    CHECK(lit == expected);
                    std::printf("  mismatch char '%c' pixel (%d,%d)\n", text[ci], px, py);
                    return;
                }
            }
        }
        // And the quad sits where the pen put it.
        CHECK(approx(topLeft.x, originX + ci * charW));
        CHECK(approx(topLeft.y, originY));
    }
}

int main() {
    testGlyphIndexing();
    testAtlasContents();
    testQuadLayout();
    testBackgroundQuad();
    testQuadsPlusAtlasReproduceFont();

    if (g_failures == 0) {
        std::printf("All debug text tests passed.\n");
        return 0;
    }
    std::printf("%d debug text test(s) failed.\n", g_failures);
    return 1;
}
