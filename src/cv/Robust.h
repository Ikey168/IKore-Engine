#pragma once

#include "cv/Image.h"
#include "cv/Rectify.h"   // Point, Quad, Mat3, computeHomography, applyHomography, warpPerspective, whiteBalance
#include "cv/Vectorize.h" // Mask, Polyline, binarizeAdaptive, denoise, thinZhangSuen, tracePolylines, douglasPeucker, snapPolyline, VectorizeOptions

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * @file Robust.h
 * @brief Phase 2 robustness for the drawing->map CV pipeline (issue #239).
 *
 * Milestone 16, "Doodle Dungeon". Phase 1 (Rectify.h / Vectorize.h / Topology.h)
 * turns a clean grid-paper photo into snapped wall polylines. Phase 2 hardens that
 * against real-world input - uneven lighting and shadows, off-angle photos, colored
 * paper, and freehand wobble - which PHONE_GAME_CONCEPT.md flags as the biggest
 * product risk. Everything here is additive: the Phase 1 functions are unchanged and
 * still available, so clean input behaves exactly as before.
 *
 * Added stages (all pure math on the dependency-free Image/Mask, no OpenCV,
 * header-only and unit-testable headless):
 *   - flattenIllumination: divide luminance by its local mean (integral-image box
 *     blur) so lighting gradients and shadows cancel before binarization.
 *   - estimateSkewAngle / deskewImage: find and remove residual rotation via the
 *     projection-profile of the ink, so near-axis walls actually snap to axis.
 *   - estimateBackgroundColor / foregroundMask / detectPaperQuadRobust: separate
 *     paper from the background by color distance (not just brightness), so colored
 *     or dark paper on a contrasting surface is still found, then keep the largest
 *     component to reject clutter.
 *   - mergeCollinear: collapse near-collinear vertices left by wobble into straight
 *     runs (beyond Douglas-Peucker's perpendicular test).
 *   - vectorizeWallsRobust / rectifyRobust: the hardened pipelines wiring it up.
 */
namespace IKore {
namespace cv {

// ---------------------------------------------------------------------------
// Illumination flattening
// ---------------------------------------------------------------------------
namespace detail {

/// Integral image (summed-area table) of luminance, size (W+1) x (H+1).
inline std::vector<double> luminanceIntegral(const Image& img) {
    const int W = img.width, H = img.height;
    std::vector<double> I(static_cast<std::size_t>(W + 1) * (H + 1), 0.0);
    for (int y = 0; y < H; ++y) {
        double rowSum = 0.0;
        for (int x = 0; x < W; ++x) {
            rowSum += img.luminance(x, y);
            I[static_cast<std::size_t>(y + 1) * (W + 1) + (x + 1)] =
                I[static_cast<std::size_t>(y) * (W + 1) + (x + 1)] + rowSum;
        }
    }
    return I;
}

/// Mean luminance over the box [x-r, x+r] x [y-r, y+r] clamped to the image.
inline float boxMean(const std::vector<double>& I, int W, int H, int x, int y, int r) {
    int x0 = x - r, y0 = y - r, x1 = x + r, y1 = y + r;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W - 1) x1 = W - 1;
    if (y1 > H - 1) y1 = H - 1;
    const int stride = W + 1;
    const double s = I[static_cast<std::size_t>(y1 + 1) * stride + (x1 + 1)] -
                     I[static_cast<std::size_t>(y0) * stride + (x1 + 1)] -
                     I[static_cast<std::size_t>(y1 + 1) * stride + x0] +
                     I[static_cast<std::size_t>(y0) * stride + x0];
    const int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    return area > 0 ? static_cast<float>(s / area) : 0.0f;
}

} // namespace detail

/**
 * @brief Flat-field the illumination: return a 1-channel image where each pixel is
 *        its luminance divided by the local background mean.
 *
 * The background mean is a large-radius box blur (via an integral image), so it
 * tracks the paper tone under any lighting. Dividing by it maps paper to ~white and
 * keeps ink dark everywhere, so a single threshold - or the adaptive one - works
 * uniformly across gradients and shadows. @p radius should be several times the
 * stroke width so ink does not pull its own background down.
 */
inline Image flattenIllumination(const Image& img, int radius = 15) {
    Image out(img.width, img.height, 1);
    if (img.width <= 0 || img.height <= 0) return out;
    const std::vector<double> I = detail::luminanceIntegral(img);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const float bg = detail::boxMean(I, img.width, img.height, x, y, radius);
            const float ratio = bg > 1.0f ? img.luminance(x, y) / bg : 1.0f;
            out.set(x, y, 0, clampByte(ratio * 255.0f));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Deskew (residual rotation removal)
// ---------------------------------------------------------------------------
namespace detail {

/// Projection "energy" of the ink after rotating coordinates by @p thetaRad:
/// the sum of squared bin counts for the row-projection plus the column-projection.
/// Axis-aligned strokes concentrate into few bins, so the energy peaks when the
/// rotation aligns the drawing to the axes.
inline double projectionEnergy(const Mask& m, float thetaRad) {
    const int W = m.width, H = m.height;
    const float c = std::cos(thetaRad), s = std::sin(thetaRad);
    const int nRows = W + H + 2;
    std::vector<int> rowHist(static_cast<std::size_t>(nRows), 0);
    std::vector<int> colHist(static_cast<std::size_t>(nRows), 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (!m.get(x, y)) continue;
            const int rb = static_cast<int>(std::lround(y * c - x * s)) + W;
            const int cb = static_cast<int>(std::lround(x * c + y * s));
            if (rb >= 0 && rb < nRows) ++rowHist[static_cast<std::size_t>(rb)];
            if (cb >= 0 && cb < nRows) ++colHist[static_cast<std::size_t>(cb)];
        }
    }
    double energy = 0.0;
    for (int i = 0; i < nRows; ++i) {
        const double r = rowHist[static_cast<std::size_t>(i)];
        const double cc = colHist[static_cast<std::size_t>(i)];
        energy += r * r + cc * cc;
    }
    return energy;
}

} // namespace detail

/**
 * @brief Estimate the rotation (degrees) that best axis-aligns the ink in @p m.
 *
 * Searches candidate angles in [-maxDeg, maxDeg] and returns the correction to pass
 * to deskewImage(): the negative of the angle that maximizes the projection energy.
 * Returns 0 for an empty mask.
 */
inline float estimateSkewAngle(const Mask& m, float maxDeg = 12.0f, float stepDeg = 0.5f) {
    if (m.count() == 0 || maxDeg <= 0.0f || stepDeg <= 0.0f) return 0.0f;
    const float toRad = 3.14159265358979323846f / 180.0f;
    float bestDeg = 0.0f;
    double bestEnergy = -1.0;
    for (float a = -maxDeg; a <= maxDeg + 1e-4f; a += stepDeg) {
        const double e = detail::projectionEnergy(m, a * toRad);
        if (e > bestEnergy) {
            bestEnergy = e;
            bestDeg = a;
        }
    }
    return -bestDeg; // correction rotates the content the other way
}

/// Rotate @p img by @p angleDeg about its center (reusing the homography warp).
inline Image deskewImage(const Image& img, float angleDeg) {
    if (std::fabs(angleDeg) < 1e-3f || img.width <= 0 || img.height <= 0) return img;
    const float toRad = 3.14159265358979323846f / 180.0f;
    const float a = angleDeg * toRad;
    const float ca = std::cos(a), sa = std::sin(a);
    const float cx = (img.width - 1) * 0.5f, cy = (img.height - 1) * 0.5f;
    // Destination -> source map: rotate the sampled coordinate by -angle about center.
    Mat3 dstToSrc;
    dstToSrc.m[0] = ca;  dstToSrc.m[1] = sa;  dstToSrc.m[2] = cx - ca * cx - sa * cy;
    dstToSrc.m[3] = -sa; dstToSrc.m[4] = ca;  dstToSrc.m[5] = cy + sa * cx - ca * cy;
    dstToSrc.m[6] = 0;   dstToSrc.m[7] = 0;   dstToSrc.m[8] = 1;
    return warpPerspective(img, dstToSrc, img.width, img.height);
}

// ---------------------------------------------------------------------------
// Robust paper detection (color-distance foreground)
// ---------------------------------------------------------------------------

/// Mean color of the @p border-wide frame ring - an estimate of the background/table.
inline std::vector<float> estimateBackgroundColor(const Image& img, int border = 2) {
    std::vector<float> bg(static_cast<std::size_t>(std::max(1, img.channels)), 0.0f);
    if (img.width <= 0 || img.height <= 0 || img.channels <= 0) return bg;
    if (border < 1) border = 1;
    long count = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const bool ring = x < border || y < border || x >= img.width - border || y >= img.height - border;
            if (!ring) continue;
            for (int c = 0; c < img.channels; ++c) bg[static_cast<std::size_t>(c)] += img.at(x, y, c);
            ++count;
        }
    }
    if (count > 0) {
        for (float& v : bg) v /= static_cast<float>(count);
    }
    return bg;
}

/// Foreground (paper) mask: pixels whose color is far from the background by more
/// than @p threshold (L1 over channels). Works for colored or dark paper, not just
/// bright paper, as long as it contrasts the surface it sits on.
inline Mask foregroundMask(const Image& img, float threshold = 60.0f, int border = 2) {
    Mask m(img.width, img.height);
    if (img.width <= 0 || img.height <= 0 || img.channels <= 0) return m;
    const std::vector<float> bg = estimateBackgroundColor(img, border);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float dist = 0.0f;
            for (int c = 0; c < img.channels; ++c) {
                dist += std::fabs(static_cast<float>(img.at(x, y, c)) - bg[static_cast<std::size_t>(c)]);
            }
            if (dist > threshold) m.set(x, y, 1);
        }
    }
    return m;
}

/// Keep only the largest 8-connected component of @p m, clearing everything else.
inline void keepLargestComponent(Mask& m) {
    Mask visited(m.width, m.height);
    const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    std::vector<std::pair<int, int>> stack, comp, best;
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            if (!m.get(x, y) || visited.get(x, y)) continue;
            stack.clear();
            comp.clear();
            stack.push_back({x, y});
            visited.set(x, y, 1);
            while (!stack.empty()) {
                const std::pair<int, int> p = stack.back();
                stack.pop_back();
                comp.push_back(p);
                for (int i = 0; i < 8; ++i) {
                    const int ax = p.first + dx8[i], ay = p.second + dy8[i];
                    if (m.get(ax, ay) && !visited.get(ax, ay)) {
                        visited.set(ax, ay, 1);
                        stack.push_back({ax, ay});
                    }
                }
            }
            if (comp.size() > best.size()) best.swap(comp);
        }
    }
    for (std::uint8_t& v : m.px) v = 0;
    for (const std::pair<int, int>& p : best) m.set(p.first, p.second, 1);
}

/**
 * @brief Detect the paper quad by color-distance foreground, robust to colored/dark
 *        paper and off-angle photos.
 *
 * Builds the foreground mask, keeps its largest component (the sheet), and takes the
 * extremes of (x+y) and (x-y) as the convex corners. Falls back to the full frame if
 * too little foreground is found.
 */
inline Quad detectPaperQuadRobust(const Image& img, float threshold = 60.0f) {
    Quad q{{0, 0},
           {static_cast<float>(img.width - 1), 0},
           {static_cast<float>(img.width - 1), static_cast<float>(img.height - 1)},
           {0, static_cast<float>(img.height - 1)}};
    if (img.width <= 1 || img.height <= 1) return q;

    Mask fg = foregroundMask(img, threshold);
    keepLargestComponent(fg);
    const int total = img.width * img.height;
    if (fg.count() < total / 100) return q; // not enough paper found; keep full frame

    bool any = false;
    float minSum = 0, maxSum = 0, minDiff = 0, maxDiff = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            if (!fg.get(x, y)) continue;
            const float sum = static_cast<float>(x + y);
            const float diff = static_cast<float>(x - y);
            const Point here{static_cast<float>(x), static_cast<float>(y)};
            if (!any) {
                minSum = maxSum = sum;
                minDiff = maxDiff = diff;
                q.tl = q.tr = q.br = q.bl = here;
                any = true;
                continue;
            }
            if (sum < minSum) { minSum = sum; q.tl = here; }
            if (sum > maxSum) { maxSum = sum; q.br = here; }
            if (diff > maxDiff) { maxDiff = diff; q.tr = here; }
            if (diff < minDiff) { minDiff = diff; q.bl = here; }
        }
    }
    return q;
}

/// Robust rectify: color-distance paper detection -> homography warp -> white balance.
inline Image rectifyRobust(const Image& photo, int outSize, float threshold = 60.0f) {
    const Quad q = detectPaperQuadRobust(photo, threshold);
    const Point dst[4] = {{0, 0},
                          {static_cast<float>(outSize), 0},
                          {static_cast<float>(outSize), static_cast<float>(outSize)},
                          {0, static_cast<float>(outSize)}};
    const Point src[4] = {q.tl, q.tr, q.br, q.bl};
    const Mat3 dstToSrc = computeHomography(dst, src);
    Image out = warpPerspective(photo, dstToSrc, outSize, outSize);
    whiteBalance(out);
    return out;
}

// ---------------------------------------------------------------------------
// Wobble-tolerant vectorization
// ---------------------------------------------------------------------------

/**
 * @brief Merge near-collinear vertices: drop any interior point where the turn
 *        between the incoming and outgoing segment is smaller than @p angleTolDeg.
 *
 * Complements Douglas-Peucker: after snapping, wobble often leaves a chain of tiny
 * turns (or two same-direction segments) that DP's perpendicular test keeps; this
 * collapses them into straight runs while preserving genuine corners. Endpoints are
 * always kept; coincident points are dropped.
 */
inline Polyline mergeCollinear(const Polyline& poly, float angleTolDeg = 12.0f) {
    if (poly.size() < 3) return poly;
    const float toDeg = 180.0f / 3.14159265358979323846f;
    Polyline out;
    out.push_back(poly.front());
    for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
        const Point& prev = out.back();
        const Point& cur = poly[i];
        const Point& next = poly[i + 1];
        const float v1x = cur.x - prev.x, v1y = cur.y - prev.y;
        const float v2x = next.x - cur.x, v2y = next.y - cur.y;
        const float l1 = std::sqrt(v1x * v1x + v1y * v1y);
        const float l2 = std::sqrt(v2x * v2x + v2y * v2y);
        if (l1 < 1e-6f) continue; // coincident with the kept point; drop it
        if (l2 < 1e-6f) continue;
        const float cross = v1x * v2y - v1y * v2x;
        const float dot = v1x * v2x + v1y * v2y;
        const float angle = std::atan2(std::fabs(cross), dot) * toDeg;
        if (angle >= angleTolDeg) out.push_back(cur); // a real corner: keep it
    }
    out.push_back(poly.back());
    return out;
}

/// Knobs for the robust pipeline; @c vec reuses the Phase 1 vectorization options.
struct RobustOptions {
    int illumRadius{15};
    bool deskew{true};
    float maxSkewDeg{12.0f};
    float collinearTolDeg{18.0f};
    float closeLoopFactor{1.5f}; ///< close a polyline if its ends are within this * grid.
    VectorizeOptions vec{};
};

/**
 * @brief Hardened wall vectorization: flatten illumination and deskew, then run the
 *        Phase 1 binarize/denoise/thin/trace, then simplify with Douglas-Peucker,
 *        collinear-merge, angle/grid snap, and a final merge; near-closed loops are
 *        closed so rooms come out as closed polygons.
 */
inline std::vector<Polyline> vectorizeWallsRobust(const Image& img, const RobustOptions& opt = {}) {
    Image work = flattenIllumination(img, opt.illumRadius);
    if (opt.deskew) {
        Mask pre = binarizeAdaptive(work, opt.vec.adaptiveRadius, opt.vec.threshC);
        denoise(pre, opt.vec.minComponentArea);
        const float skew = estimateSkewAngle(pre, opt.maxSkewDeg);
        if (std::fabs(skew) > 0.25f) work = deskewImage(work, skew);
    }

    Mask bin = binarizeAdaptive(work, opt.vec.adaptiveRadius, opt.vec.threshC);
    denoise(bin, opt.vec.minComponentArea);
    const Mask skeleton = thinZhangSuen(bin);
    const std::vector<Polyline> traced = tracePolylines(skeleton);

    std::vector<Polyline> out;
    for (const Polyline& p : traced) {
        if (p.size() < 2) continue;
        Polyline simplified = douglasPeucker(p, opt.vec.simplifyEps);
        Polyline merged = mergeCollinear(simplified, opt.collinearTolDeg);
        Polyline snapped = snapPolyline(merged, opt.vec.gridSize);
        Polyline clean = mergeCollinear(snapped, opt.collinearTolDeg);
        if (clean.size() < 2) continue;
        // Close a loop whose endpoints landed within a cell of each other, so the
        // topology sees an enclosed room rather than a hairline gap.
        const Point& a = clean.front();
        const Point& b = clean.back();
        const float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        if (d > 1e-3f && d <= opt.closeLoopFactor * opt.vec.gridSize) clean.push_back(a);
        out.push_back(std::move(clean));
    }
    return out;
}

} // namespace cv
} // namespace IKore
