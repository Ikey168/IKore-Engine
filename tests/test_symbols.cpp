// Test for Tier-1 symbol recognition - Milestone 16, #169.
//
// Verifies the issue's acceptance criteria: Tier-1 symbols (start/exit/coin/enemy)
// are recognized by color class (+ shape subtype) and mapped to the correct game
// objects, and an unknown/low-confidence mark falls back to a sensible default
// rather than failing.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_symbols.cpp -o test_symbols

#include "cv/Image.h"
#include "cv/Symbols.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore::cv;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static void fillRect(Image& im, int cx, int cy, int r, int R, int G, int B, int c) {
    (void)c;
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (im.inBounds(x, y)) {
                im.set(x, y, 0, static_cast<std::uint8_t>(R));
                im.set(x, y, 1, static_cast<std::uint8_t>(G));
                im.set(x, y, 2, static_cast<std::uint8_t>(B));
            }
        }
    }
}

static void fillCircle(Image& im, int cx, int cy, int r, int R, int G, int B) {
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r && im.inBounds(x, y)) {
                im.set(x, y, 0, static_cast<std::uint8_t>(R));
                im.set(x, y, 1, static_cast<std::uint8_t>(G));
                im.set(x, y, 2, static_cast<std::uint8_t>(B));
            }
        }
    }
}

static void putRGB(Image& im, int x, int y, int R, int G, int B) {
    if (!im.inBounds(x, y)) return;
    im.set(x, y, 0, static_cast<std::uint8_t>(R));
    im.set(x, y, 1, static_cast<std::uint8_t>(G));
    im.set(x, y, 2, static_cast<std::uint8_t>(B));
}

static void drawX(Image& im, int cx, int cy, int r, int R, int G, int B) {
    for (int t = -r; t <= r; ++t) {
        for (int w = -1; w <= 1; ++w) {
            putRGB(im, cx + t + w, cy + t, R, G, B); // one diagonal
            putRGB(im, cx + t + w, cy - t, R, G, B); // the other
        }
    }
}

static void testClassifyColor() {
    CHECK(classifyColor(30, 180, 30).cls == ColorClass::Green);
    CHECK(classifyColor(200, 30, 30).cls == ColorClass::Red);
    CHECK(classifyColor(235, 215, 20).cls == ColorClass::Yellow);
    CHECK(classifyColor(30, 60, 210).cls == ColorClass::Blue);
    CHECK(classifyColor(30, 180, 30).confidence > 0.5f);
    // Gray / dark marks are not a color class.
    CHECK(classifyColor(150, 150, 150).cls == ColorClass::Unknown);
    CHECK(classifyColor(20, 20, 25).cls == ColorClass::Unknown);
}

static void testClassifyShape() {
    CHECK(classifyShape(14, 14, 196).cls == ShapeClass::Square);   // fill 1.0
    CHECK(classifyShape(16, 16, 201).cls == ShapeClass::Circle);   // fill ~0.785
    CHECK(classifyShape(20, 20, 200).cls == ShapeClass::Triangle); // fill 0.5
    CHECK(classifyShape(20, 20, 140).cls == ShapeClass::Cross);    // fill ~0.35
}

static void testRecognizeMapsToGameObjects() {
    CHECK(recognizeSymbol(30, 180, 30, 16, 16, 200).object == "player"); // start
    CHECK(recognizeSymbol(200, 30, 30, 18, 18, 120).object == "enemy");
    CHECK(recognizeSymbol(235, 215, 20, 16, 16, 200).object == "coin");
    CHECK(recognizeSymbol(30, 60, 210, 14, 14, 196).object == "exit");
    CHECK(!recognizeSymbol(30, 180, 30, 16, 16, 200).lowConfidence);
}

static void testUnknownFallsBackToDefault() {
    const SymbolResult gray = recognizeSymbol(150, 150, 150, 16, 16, 200, "coin");
    CHECK(gray.object == "coin");   // sensible default, not a failure
    CHECK(gray.lowConfidence);
    CHECK(gray.color == ColorClass::Unknown);

    // The default is configurable.
    const SymbolResult custom = recognizeSymbol(150, 150, 150, 16, 16, 200, "player");
    CHECK(custom.object == "player");
    CHECK(custom.lowConfidence);
}

static const SymbolInstance* nearest(const std::vector<SymbolInstance>& syms, float x, float y) {
    const SymbolInstance* best = nullptr;
    float bestD = 1e9f;
    for (const SymbolInstance& s : syms) {
        const float d = (s.cx - x) * (s.cx - x) + (s.cy - y) * (s.cy - y);
        if (d < bestD) {
            bestD = d;
            best = &s;
        }
    }
    return best;
}

static void testDetectSymbolsEndToEnd() {
    Image img(110, 60, 3);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < 3; ++c) img.set(x, y, c, 250); // white paper
        }
    }
    fillCircle(img, 15, 30, 8, 30, 180, 30);   // green start
    drawX(img, 42, 30, 9, 200, 30, 30);        // red enemy
    fillCircle(img, 70, 30, 8, 235, 215, 20);  // yellow coin
    fillRect(img, 96, 30, 7, 30, 60, 210, 0);  // blue exit

    const std::vector<SymbolInstance> syms = detectSymbols(img);
    CHECK(syms.size() == 4);

    const SymbolInstance* start = nearest(syms, 15, 30);
    const SymbolInstance* enemy = nearest(syms, 42, 30);
    const SymbolInstance* coin = nearest(syms, 70, 30);
    const SymbolInstance* exit = nearest(syms, 96, 30);
    CHECK(start && start->result.object == "player");
    CHECK(enemy && enemy->result.object == "enemy");
    CHECK(coin && coin->result.object == "coin");
    CHECK(exit && exit->result.object == "exit");
    // Centroids land on the drawn marks.
    CHECK(start && std::fabs(start->cx - 15.0f) < 3.0f);
    CHECK(exit && std::fabs(exit->cx - 96.0f) < 3.0f);
}

int main() {
    testClassifyColor();
    testClassifyShape();
    testRecognizeMapsToGameObjects();
    testUnknownFallsBackToDefault();
    testDetectSymbolsEndToEnd();

    if (g_failures == 0) {
        std::printf("All symbol recognition tests passed.\n");
        return 0;
    }
    std::printf("%d symbol recognition test(s) failed.\n", g_failures);
    return 1;
}
