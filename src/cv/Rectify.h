#pragma once

#include "cv/Image.h"

#include <cmath>
#include <cstdint>

/**
 * @file Rectify.h
 * @brief Paper detection + perspective rectification + white balance (issue #166).
 *
 * Milestone 16 (Drawing -> Map CV). Turns a photo of a hand drawing taken at an
 * angle into a clean, square, top-down image:
 *   1. detectPaperQuad - find the bright paper quadrilateral on a darker
 *      background (luminance threshold + extreme-corner selection).
 *   2. computeHomography / warpPerspective - solve the 4-point homography and
 *      inverse-map the photo into a square (this is the rectification + auto-crop).
 *   3. whiteBalance - white-patch scaling so the paper reads as white.
 *   4. rectify - the full pipeline.
 *
 * Pure math on the dependency-free Image (no OpenCV), so it is unit-testable
 * headless. Header-only.
 */
namespace IKore {
namespace cv {

struct Point {
    float x{0.0f};
    float y{0.0f};
};

/// Paper corners in image space, ordered top-left, top-right, bottom-right, bottom-left.
struct Quad {
    Point tl, tr, br, bl;
};

/// 3x3 homography, row-major. Maps (x,y,1) -> (X,Y) after the perspective divide.
struct Mat3 {
    double m[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
};

namespace detail {

/// Solve an n x n linear system A x = b in place (Gaussian elimination, pivoting).
inline bool solveLinear(int n, double* A, double* b, double* x) {
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        double best = std::fabs(A[col * n + col]);
        for (int r = col + 1; r < n; ++r) {
            const double v = std::fabs(A[r * n + col]);
            if (v > best) {
                best = v;
                pivot = r;
            }
        }
        if (best < 1e-12) return false; // singular
        if (pivot != col) {
            for (int k = 0; k < n; ++k) {
                const double t = A[col * n + k];
                A[col * n + k] = A[pivot * n + k];
                A[pivot * n + k] = t;
            }
            const double tb = b[col];
            b[col] = b[pivot];
            b[pivot] = tb;
        }
        const double diag = A[col * n + col];
        for (int r = 0; r < n; ++r) {
            if (r == col) continue;
            const double f = A[r * n + col] / diag;
            for (int k = col; k < n; ++k) A[r * n + k] -= f * A[col * n + k];
            b[r] -= f * b[col];
        }
    }
    for (int i = 0; i < n; ++i) x[i] = b[i] / A[i * n + i];
    return true;
}

} // namespace detail

/// Homography mapping the four @p from points to the four @p to points.
inline Mat3 computeHomography(const Point from[4], const Point to[4]) {
    double A[64] = {0};
    double b[8] = {0};
    for (int i = 0; i < 4; ++i) {
        const double x = from[i].x, y = from[i].y, X = to[i].x, Y = to[i].y;
        double* r0 = &A[(2 * i) * 8];
        r0[0] = x; r0[1] = y; r0[2] = 1; r0[6] = -x * X; r0[7] = -y * X;
        b[2 * i] = X;
        double* r1 = &A[(2 * i + 1) * 8];
        r1[3] = x; r1[4] = y; r1[5] = 1; r1[6] = -x * Y; r1[7] = -y * Y;
        b[2 * i + 1] = Y;
    }
    double h[8] = {0};
    Mat3 out;
    if (!detail::solveLinear(8, A, b, h)) return out; // identity on degenerate input
    for (int i = 0; i < 8; ++i) out.m[i] = h[i];
    out.m[8] = 1.0;
    return out;
}

/// Apply a homography to a point (with the perspective divide).
inline Point applyHomography(const Mat3& h, float x, float y) {
    const double denom = h.m[6] * x + h.m[7] * y + h.m[8];
    const double inv = std::fabs(denom) > 1e-12 ? 1.0 / denom : 0.0;
    return Point{static_cast<float>((h.m[0] * x + h.m[1] * y + h.m[2]) * inv),
                 static_cast<float>((h.m[3] * x + h.m[4] * y + h.m[5]) * inv)};
}

/**
 * @brief Detect the bright paper quad on a darker background.
 *
 * Pixels brighter than @p threshFrac of the peak luminance are treated as paper;
 * the quad corners are the extremes of (x+y) and (x-y) over those pixels (a robust
 * convex-corner pick for a roughly rectangular sheet).
 */
inline Quad detectPaperQuad(const Image& img, float threshFrac = 0.5f) {
    Quad q{{0, 0},
           {static_cast<float>(img.width - 1), 0},
           {static_cast<float>(img.width - 1), static_cast<float>(img.height - 1)},
           {0, static_cast<float>(img.height - 1)}};
    if (img.width <= 1 || img.height <= 1) return q;

    float maxLum = 0.0f;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) maxLum = std::fmax(maxLum, img.luminance(x, y));
    }
    const float thresh = threshFrac * maxLum;

    bool any = false;
    float minSum = 0, maxSum = 0, minDiff = 0, maxDiff = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            if (img.luminance(x, y) <= thresh) continue;
            const float sum = static_cast<float>(x + y);
            const float diff = static_cast<float>(x - y);
            if (!any) {
                minSum = maxSum = sum;
                minDiff = maxDiff = diff;
                q.tl = q.br = q.tr = q.bl = Point{static_cast<float>(x), static_cast<float>(y)};
                any = true;
                continue;
            }
            if (sum < minSum) { minSum = sum; q.tl = {static_cast<float>(x), static_cast<float>(y)}; }
            if (sum > maxSum) { maxSum = sum; q.br = {static_cast<float>(x), static_cast<float>(y)}; }
            if (diff > maxDiff) { maxDiff = diff; q.tr = {static_cast<float>(x), static_cast<float>(y)}; }
            if (diff < minDiff) { minDiff = diff; q.bl = {static_cast<float>(x), static_cast<float>(y)}; }
        }
    }
    return q;
}

/// Warp @p src into an @p outW x @p outH image using a destination->source map.
inline Image warpPerspective(const Image& src, const Mat3& dstToSrc, int outW, int outH) {
    Image out(outW, outH, src.channels);
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            const Point s = applyHomography(dstToSrc, static_cast<float>(x), static_cast<float>(y));
            for (int c = 0; c < src.channels; ++c) {
                out.set(x, y, c, clampByte(src.sample(s.x, s.y, c)));
            }
        }
    }
    return out;
}

/// White-patch white balance: scale each channel so its peak maps to white.
inline void whiteBalance(Image& img) {
    if (img.channels <= 0 || img.data.empty()) return;
    std::vector<float> peak(static_cast<std::size_t>(img.channels), 1.0f);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < img.channels; ++c) {
                peak[static_cast<std::size_t>(c)] =
                    std::fmax(peak[static_cast<std::size_t>(c)], static_cast<float>(img.at(x, y, c)));
            }
        }
    }
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < img.channels; ++c) {
                const float scale = 255.0f / peak[static_cast<std::size_t>(c)];
                img.set(x, y, c, clampByte(img.at(x, y, c) * scale));
            }
        }
    }
}

/**
 * @brief Rectify a photo to a clean @p outSize x @p outSize top-down image:
 *        detect the paper, perspective-warp it to a square, and white-balance.
 */
inline Image rectify(const Image& photo, int outSize) {
    const Quad q = detectPaperQuad(photo);
    const Point dst[4] = {{0, 0},
                          {static_cast<float>(outSize), 0},
                          {static_cast<float>(outSize), static_cast<float>(outSize)},
                          {0, static_cast<float>(outSize)}};
    const Point src[4] = {q.tl, q.tr, q.br, q.bl};
    const Mat3 dstToSrc = computeHomography(dst, src); // inverse map for sampling
    Image out = warpPerspective(photo, dstToSrc, outSize, outSize);
    whiteBalance(out);
    return out;
}

} // namespace cv
} // namespace IKore
