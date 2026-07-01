#pragma once

#include "cv/Image.h"
#include "cv/Rectify.h" // cv::Point

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * @file Vectorize.h
 * @brief Binarize/clean + wall vectorization with angle/grid snap (issue #167).
 *
 * Milestone 16. Turns a (rectified) grayscale drawing of hand-drawn wall lines
 * into clean, snapped vector polylines:
 *   grayscale -> adaptive threshold -> denoise (drop small specks) -> thin
 *   (Zhang-Suen skeleton) -> trace polylines -> simplify (Douglas-Peucker) ->
 *   snap each segment to 0/45/90 degrees and the grid.
 *
 * Pure math on the dependency-free Image (no OpenCV), so it is unit-testable
 * headless. The resulting polylines feed the M15 LevelSpec / scene converter.
 * Header-only.
 */
namespace IKore {
namespace cv {

using Polyline = std::vector<Point>;

/// A binary ink mask (1 = ink, 0 = background).
struct Mask {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> px;

    Mask(int w, int h) : width(w), height(h), px(static_cast<std::size_t>(w) * h, 0) {}
    std::uint8_t get(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return px[static_cast<std::size_t>(y) * width + x];
    }
    void set(int x, int y, std::uint8_t v) {
        if (x >= 0 && y >= 0 && x < width && y < height) px[static_cast<std::size_t>(y) * width + x] = v;
    }
    int count() const {
        int n = 0;
        for (std::uint8_t v : px) n += (v != 0);
        return n;
    }
};

/// Adaptive threshold: ink where a pixel is darker than its local mean minus @p c.
inline Mask binarizeAdaptive(const Image& img, int radius = 6, float c = 10.0f) {
    Mask m(img.width, img.height);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float sum = 0.0f;
            int n = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int xx = x + dx, yy = y + dy;
                    if (img.inBounds(xx, yy)) {
                        sum += img.luminance(xx, yy);
                        ++n;
                    }
                }
            }
            const float mean = n > 0 ? sum / n : 0.0f;
            if (img.luminance(x, y) < mean - c) m.set(x, y, 1);
        }
    }
    return m;
}

/// Remove connected ink components smaller than @p minArea (8-connected).
inline void denoise(Mask& m, int minArea = 12) {
    Mask visited(m.width, m.height);
    const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    std::vector<std::pair<int, int>> stack, comp;
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
            if (static_cast<int>(comp.size()) < minArea) {
                for (const std::pair<int, int>& p : comp) m.set(p.first, p.second, 0);
            }
        }
    }
}

/// Zhang-Suen thinning: reduce ink strokes to a 1-pixel-wide skeleton.
inline Mask thinZhangSuen(const Mask& input) {
    Mask m = input;
    std::vector<std::pair<int, int>> toDelete;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int step = 0; step < 2; ++step) {
            toDelete.clear();
            for (int y = 1; y < m.height - 1; ++y) {
                for (int x = 1; x < m.width - 1; ++x) {
                    if (!m.get(x, y)) continue;
                    const int p2 = m.get(x, y - 1), p3 = m.get(x + 1, y - 1), p4 = m.get(x + 1, y),
                              p5 = m.get(x + 1, y + 1), p6 = m.get(x, y + 1), p7 = m.get(x - 1, y + 1),
                              p8 = m.get(x - 1, y), p9 = m.get(x - 1, y - 1);
                    const int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                    if (b < 2 || b > 6) continue;
                    const int seq[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
                    int a = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (seq[i] == 0 && seq[i + 1] == 1) ++a;
                    }
                    if (a != 1) continue;
                    if (step == 0) {
                        if (p2 * p4 * p6 != 0) continue;
                        if (p4 * p6 * p8 != 0) continue;
                    } else {
                        if (p2 * p4 * p8 != 0) continue;
                        if (p2 * p6 * p8 != 0) continue;
                    }
                    toDelete.push_back({x, y});
                }
            }
            if (!toDelete.empty()) {
                changed = true;
                for (const std::pair<int, int>& p : toDelete) m.set(p.first, p.second, 0);
            }
        }
    }
    return m;
}

/// Trace a skeleton into polylines (greedy walk, orthogonal neighbours first).
inline std::vector<Polyline> tracePolylines(const Mask& sk) {
    std::vector<Polyline> result;
    Mask visited(sk.width, sk.height);
    const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    auto degree = [&](int x, int y) {
        int d = 0;
        for (int i = 0; i < 8; ++i) {
            if (sk.get(x + dx8[i], y + dy8[i])) ++d;
        }
        return d;
    };
    auto walk = [&](int sx, int sy) {
        Polyline poly;
        int cx = sx, cy = sy;
        while (true) {
            poly.push_back(Point{static_cast<float>(cx), static_cast<float>(cy)});
            visited.set(cx, cy, 1);
            int nx = -1, ny = -1;
            for (int i = 0; i < 8; ++i) {
                const int ax = cx + dx8[i], ay = cy + dy8[i];
                if (sk.get(ax, ay) && !visited.get(ax, ay)) {
                    nx = ax;
                    ny = ay;
                    break;
                }
            }
            if (nx < 0) break;
            cx = nx;
            cy = ny;
        }
        return poly;
    };
    // Open paths from endpoints first, then any remaining loops.
    for (int y = 0; y < sk.height; ++y) {
        for (int x = 0; x < sk.width; ++x) {
            if (sk.get(x, y) && !visited.get(x, y) && degree(x, y) == 1) {
                Polyline p = walk(x, y);
                if (p.size() >= 2) result.push_back(std::move(p));
            }
        }
    }
    for (int y = 0; y < sk.height; ++y) {
        for (int x = 0; x < sk.width; ++x) {
            if (sk.get(x, y) && !visited.get(x, y)) {
                Polyline p = walk(x, y);
                if (p.size() >= 2) result.push_back(std::move(p));
            }
        }
    }
    return result;
}

namespace detail {
inline void dpRecurse(const Polyline& pts, int i, int j, float eps, std::vector<char>& keep) {
    if (j <= i + 1) return;
    const float x1 = pts[i].x, y1 = pts[i].y, x2 = pts[j].x, y2 = pts[j].y;
    const float dx = x2 - x1, dy = y2 - y1;
    const float len = std::sqrt(dx * dx + dy * dy);
    float maxDist = -1.0f;
    int idx = -1;
    for (int k = i + 1; k < j; ++k) {
        float dist;
        if (len < 1e-6f) {
            const float ax = pts[k].x - x1, ay = pts[k].y - y1;
            dist = std::sqrt(ax * ax + ay * ay);
        } else {
            dist = std::fabs((pts[k].x - x1) * dy - (pts[k].y - y1) * dx) / len;
        }
        if (dist > maxDist) {
            maxDist = dist;
            idx = k;
        }
    }
    if (maxDist > eps && idx > 0) {
        keep[static_cast<std::size_t>(idx)] = 1;
        dpRecurse(pts, i, idx, eps, keep);
        dpRecurse(pts, idx, j, eps, keep);
    }
}
} // namespace detail

/// Douglas-Peucker simplification (also merges collinear runs of points).
inline Polyline douglasPeucker(const Polyline& pts, float eps) {
    if (pts.size() < 3) return pts;
    std::vector<char> keep(pts.size(), 0);
    keep.front() = 1;
    keep.back() = 1;
    detail::dpRecurse(pts, 0, static_cast<int>(pts.size()) - 1, eps, keep);
    Polyline out;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (keep[i]) out.push_back(pts[i]);
    }
    return out;
}

namespace detail {
inline float snapValue(float v, float grid) { return grid > 0.0f ? std::round(v / grid) * grid : v; }
} // namespace detail

/// Snap a polyline: each segment to 0/45/90 degrees and each vertex to the grid.
inline Polyline snapPolyline(const Polyline& pts, float grid) {
    if (pts.empty()) return pts;
    Polyline out;
    Point prev{detail::snapValue(pts[0].x, grid), detail::snapValue(pts[0].y, grid)};
    out.push_back(prev);
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const float dx = pts[i].x - prev.x, dy = pts[i].y - prev.y;
        const float adx = std::fabs(dx), ady = std::fabs(dy);
        Point cur = pts[i];
        if (adx >= 2.0f * ady) {
            cur.y = prev.y; // horizontal (0 degrees)
        } else if (ady >= 2.0f * adx) {
            cur.x = prev.x; // vertical (90 degrees)
        } else {
            const float m = (adx + ady) * 0.5f; // diagonal (45 degrees)
            cur.x = prev.x + (dx < 0 ? -m : m);
            cur.y = prev.y + (dy < 0 ? -m : m);
        }
        cur.x = detail::snapValue(cur.x, grid);
        cur.y = detail::snapValue(cur.y, grid);
        if (!(cur.x == prev.x && cur.y == prev.y)) out.push_back(cur);
        prev = cur;
    }
    return out;
}

struct VectorizeOptions {
    int adaptiveRadius{6};
    float threshC{10.0f};
    int minComponentArea{12};
    float simplifyEps{2.5f};
    float gridSize{10.0f};
};

/// Full pipeline: a grayscale drawing -> clean, snapped wall polylines.
inline std::vector<Polyline> vectorizeWalls(const Image& img, const VectorizeOptions& opt = {}) {
    Mask bin = binarizeAdaptive(img, opt.adaptiveRadius, opt.threshC);
    denoise(bin, opt.minComponentArea);
    const Mask skeleton = thinZhangSuen(bin);
    const std::vector<Polyline> traced = tracePolylines(skeleton);

    std::vector<Polyline> out;
    for (const Polyline& p : traced) {
        if (p.size() < 2) continue;
        Polyline simplified = douglasPeucker(p, opt.simplifyEps);
        Polyline snapped = snapPolyline(simplified, opt.gridSize);
        if (snapped.size() >= 2) out.push_back(std::move(snapped));
    }
    return out;
}

} // namespace cv
} // namespace IKore
