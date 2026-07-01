// Test for binarize/clean + wall vectorization - Milestone 16, #167.
//
// Verifies the issue's acceptance criterion: hand-drawn wall lines become clean,
// snapped vector polylines. Covers adaptive thresholding (beats a lighting
// gradient), speck denoising, Douglas-Peucker simplification, angle/grid snapping,
// and the full pipeline on a hand-drawn-style "L" wall.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_vectorize.cpp -o test_vectorize

#include "cv/Image.h"
#include "cv/Vectorize.h"

#include <cmath>
#include <cstdio>

using namespace IKore::cv;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool isMultipleOf(float v, float grid) {
    return std::fabs(v / grid - std::round(v / grid)) < 1e-3f;
}

static void testAdaptiveThresholdBeatsGradient() {
    // Paper brightens left-to-right; a dark horizontal line crosses the gradient.
    Image img(40, 40, 1);
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 40; ++x) {
            const float base = 100.0f + (static_cast<float>(x) / 39.0f) * 120.0f; // 100..220
            float v = base;
            if (y >= 19 && y <= 21) v = base - 90.0f; // ink line
            img.set(x, y, 0, clampByte(v));
        }
    }
    const Mask bin = binarizeAdaptive(img, 6, 10.0f);
    // The line is detected as ink on both the dark and bright sides...
    CHECK(bin.get(5, 20) == 1);
    CHECK(bin.get(35, 20) == 1);
    // ...while the paper is not, on both sides (a global threshold could not).
    CHECK(bin.get(5, 5) == 0);
    CHECK(bin.get(35, 5) == 0);
}

static void testDenoiseRemovesSpecks() {
    Mask m(40, 40);
    for (int y = 19; y <= 21; ++y) {
        for (int x = 5; x <= 35; ++x) m.set(x, y, 1); // a substantial line
    }
    m.set(2, 2, 1); // isolated specks
    m.set(3, 2, 1);
    denoise(m, 10);
    CHECK(m.get(2, 2) == 0);   // speck removed
    CHECK(m.get(3, 2) == 0);
    CHECK(m.get(20, 20) == 1); // line kept
}

static void testDouglasPeucker() {
    Polyline pts;
    for (int x = 0; x <= 10; ++x) pts.push_back(Point{static_cast<float>(x), 0.0f});
    for (int y = 1; y <= 10; ++y) pts.push_back(Point{10.0f, static_cast<float>(y)});
    const Polyline simple = douglasPeucker(pts, 1.0f);
    CHECK(simple.size() == 3); // start, corner, end
    CHECK(simple[1].x == 10.0f && simple[1].y == 0.0f);
}

static void testSnapPolyline() {
    const Polyline in = {{1, 2}, {31, 5}, {33, 34}};
    const Polyline out = snapPolyline(in, 10.0f);
    CHECK(out.size() == 3);
    CHECK(out[0].y == out[1].y); // first segment horizontal
    CHECK(out[1].x == out[2].x); // second segment vertical
    for (const Point& p : out) {
        CHECK(isMultipleOf(p.x, 10.0f) && isMultipleOf(p.y, 10.0f));
    }
}

static void testVectorizeHandDrawnL() {
    // A hand-drawn-style thick "L": a horizontal stroke and a vertical stroke.
    Image img(60, 60, 1);
    for (int y = 0; y < 60; ++y) {
        for (int x = 0; x < 60; ++x) img.set(x, y, 0, 240); // paper
    }
    for (int y = 19; y <= 21; ++y) {
        for (int x = 10; x <= 40; ++x) img.set(x, y, 0, 20); // horizontal stroke
    }
    for (int y = 19; y <= 40; ++y) {
        for (int x = 38; x <= 40; ++x) img.set(x, y, 0, 20); // vertical stroke
    }

    VectorizeOptions opt;
    opt.minComponentArea = 10;
    opt.gridSize = 10.0f;
    const std::vector<Polyline> walls = vectorizeWalls(img, opt);
    CHECK(!walls.empty());

    // Take the longest polyline (the L) and check it is clean and snapped.
    const Polyline* best = nullptr;
    for (const Polyline& p : walls) {
        if (best == nullptr || p.size() > best->size()) best = &p;
    }
    CHECK(best != nullptr);
    if (best == nullptr) return;

    CHECK(best->size() >= 3); // at least two segments meeting at a corner

    bool allAxisAligned = true, allOnGrid = true;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (std::size_t i = 0; i < best->size(); ++i) {
        const Point& p = (*best)[i];
        if (!isMultipleOf(p.x, 10.0f) || !isMultipleOf(p.y, 10.0f)) allOnGrid = false;
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        if (i > 0) {
            const Point& q = (*best)[i - 1];
            const bool axis = (std::fabs(p.x - q.x) < 1e-3f) || (std::fabs(p.y - q.y) < 1e-3f);
            if (!axis) allAxisAligned = false;
        }
    }
    CHECK(allAxisAligned);         // every segment is horizontal or vertical (snapped)
    CHECK(allOnGrid);              // every vertex sits on the grid
    CHECK(maxX - minX >= 20.0f);   // spans the horizontal stroke
    CHECK(maxY - minY >= 15.0f);   // spans the vertical stroke
}

int main() {
    testAdaptiveThresholdBeatsGradient();
    testDenoiseRemovesSpecks();
    testDouglasPeucker();
    testSnapPolyline();
    testVectorizeHandDrawnL();

    if (g_failures == 0) {
        std::printf("All vectorization tests passed.\n");
        return 0;
    }
    std::printf("%d vectorization test(s) failed.\n", g_failures);
    return 1;
}
