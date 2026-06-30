// Test for camera capture + perspective rectification - Milestone 16, #166.
//
// Verifies the issue's acceptance criterion: a photo taken at an angle is
// rectified to a square, legible top-down view. A synthetic "photo" is built by
// projecting a known paper pattern (white sheet with an inset corner marker) onto
// an angled quad on a dark background, with a color tint; rectify() must recover a
// square, white-balanced, correctly-oriented top-down image.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_rectify.cpp -o test_rectify

#include "cv/Image.h"
#include "cv/Rectify.h"

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

static bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

static void testHomographyRoundTrip() {
    const Point from[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    const Point to[4] = {{2, 1}, {12, 2}, {11, 13}, {1, 11}};
    const Mat3 h = computeHomography(from, to);
    for (int i = 0; i < 4; ++i) {
        const Point p = applyHomography(h, from[i].x, from[i].y);
        CHECK(near(p.x, to[i].x, 1e-3f));
        CHECK(near(p.y, to[i].y, 1e-3f));
    }
}

static void testDetectPaperQuad() {
    // A bright quad on a dark background; corners deliberately not axis-aligned.
    Image img(120, 120, 1);
    const Point corners[4] = {{20, 12}, {100, 28}, {92, 104}, {14, 96}};
    const Point dst[4] = {{0, 0}, {80, 0}, {80, 80}, {0, 80}};
    const Mat3 imgToUnit = computeHomography(corners, dst);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const Point u = applyHomography(imgToUnit, static_cast<float>(x), static_cast<float>(y));
            if (u.x >= 0 && u.x < 80 && u.y >= 0 && u.y < 80) img.set(x, y, 0, 240); // paper
        }
    }
    const Quad q = detectPaperQuad(img);
    CHECK(near(q.tl.x, 20, 4) && near(q.tl.y, 12, 4));
    CHECK(near(q.tr.x, 100, 4) && near(q.tr.y, 28, 4));
    CHECK(near(q.br.x, 92, 4) && near(q.br.y, 104, 4));
    CHECK(near(q.bl.x, 14, 4) && near(q.bl.y, 96, 4));
}

static void testWhiteBalance() {
    Image img(8, 8, 3);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            img.set(x, y, 0, 200); // a flat, tinted gray
            img.set(x, y, 1, 170);
            img.set(x, y, 2, 140);
        }
    }
    whiteBalance(img);
    // Each channel's peak is scaled to white, so the flat patch becomes ~white.
    CHECK(img.at(4, 4, 0) >= 254);
    CHECK(img.at(4, 4, 1) >= 254);
    CHECK(img.at(4, 4, 2) >= 254);
}

// A clean 80x80 "paper": white sheet with an inset black marker near the top-left.
static Image makePattern() {
    Image pat(80, 80, 3);
    for (int y = 0; y < 80; ++y) {
        for (int x = 0; x < 80; ++x) {
            for (int c = 0; c < 3; ++c) pat.set(x, y, c, 255);
        }
    }
    for (int y = 8; y < 24; ++y) {
        for (int x = 8; x < 24; ++x) {
            for (int c = 0; c < 3; ++c) pat.set(x, y, c, 0); // corner marker
        }
    }
    return pat;
}

static void testRectifyAngledPhoto() {
    const Image pat = makePattern();

    // Build the angled photo: paint the pattern onto a convex quad over a dark,
    // color-tinted background.
    Image photo(220, 220, 3);
    for (int y = 0; y < photo.height; ++y) {
        for (int x = 0; x < photo.width; ++x) {
            photo.set(x, y, 0, 28);
            photo.set(x, y, 1, 30);
            photo.set(x, y, 2, 26);
        }
    }
    const Point quad[4] = {{45, 35}, {180, 60}, {165, 195}, {35, 170}};
    const Point patCorners[4] = {{0, 0}, {80, 0}, {80, 80}, {0, 80}};
    const Mat3 photoToPat = computeHomography(quad, patCorners);
    const float tint[3] = {0.82f, 0.86f, 0.72f};
    for (int y = 0; y < photo.height; ++y) {
        for (int x = 0; x < photo.width; ++x) {
            const Point p = applyHomography(photoToPat, static_cast<float>(x), static_cast<float>(y));
            if (p.x >= 0 && p.x < 80 && p.y >= 0 && p.y < 80) {
                for (int c = 0; c < 3; ++c) photo.set(x, y, c, clampByte(pat.sample(p.x, p.y, c) * tint[c]));
            }
        }
    }

    // Detection should recover the paper corners.
    const Quad q = detectPaperQuad(photo);
    CHECK(near(q.tl.x, 45, 8) && near(q.tl.y, 35, 8));
    CHECK(near(q.br.x, 165, 8) && near(q.br.y, 195, 8));

    // Rectify to a clean 80x80 top-down square.
    const Image out = rectify(photo, 80);
    CHECK(out.width == 80 && out.height == 80 && out.channels == 3);

    // Legible + white-balanced: the paper reads white, the marker reads dark, and
    // the marker is in the top-left (orientation preserved).
    CHECK(out.luminance(40, 40) > 180.0f);  // center is white
    CHECK(out.luminance(16, 16) < 110.0f);  // marker (top-left) is dark
    CHECK(out.luminance(64, 16) > 150.0f);  // top-right is white
    CHECK(out.luminance(16, 64) > 150.0f);  // bottom-left is white
    CHECK(out.luminance(64, 64) > 150.0f);  // bottom-right is white
    CHECK(out.luminance(5, 5) > 150.0f);    // corner border is white (marker is inset)
}

int main() {
    testHomographyRoundTrip();
    testDetectPaperQuad();
    testWhiteBalance();
    testRectifyAngledPhoto();

    if (g_failures == 0) {
        std::printf("All rectification tests passed.\n");
        return 0;
    }
    std::printf("%d rectification test(s) failed.\n", g_failures);
    return 1;
}
