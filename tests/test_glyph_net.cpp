// Test for on-device symbol ML - Milestone 17, #173.
//
// Verifies the issue's acceptance criteria: recognition is robust on varied hand
// drawings (the classifier trains on randomized freehand-style glyphs and is
// evaluated on held-out varied glyphs), and inference runs within a small latency
// budget (a linear forward pass, timed here). Also checks normalization invariance
// and the low-confidence fallback.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_glyph_net.cpp -o test_glyph_net

#include "core/sim/Simulation.h"
#include "cv/Vectorize.h"
#include "ml/GlyphNet.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
using IKore::sim::DeterministicRng;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

enum Glyph { Circle = 0, Square = 1, Triangle = 2, Cross = 3 };

static void stamp(cv::Mask& m, float fx, float fy, int th) {
    const int cx = static_cast<int>(std::lround(fx)), cy = static_cast<int>(std::lround(fy));
    for (int dy = -th; dy <= th; ++dy) {
        for (int dx = -th; dx <= th; ++dx) m.set(cx + dx, cy + dy, 1);
    }
}
static void line(cv::Mask& m, float x0, float y0, float x1, float y1, int th) {
    int ix0 = static_cast<int>(std::lround(x0)), iy0 = static_cast<int>(std::lround(y0));
    const int ix1 = static_cast<int>(std::lround(x1)), iy1 = static_cast<int>(std::lround(y1));
    const int dx = std::abs(ix1 - ix0), dy = -std::abs(iy1 - iy0);
    const int sx = ix0 < ix1 ? 1 : -1, sy = iy0 < iy1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        stamp(m, static_cast<float>(ix0), static_cast<float>(iy0), th);
        if (ix0 == ix1 && iy0 == iy1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += sx; }
        if (e2 <= dx) { err += dx; iy0 += sy; }
    }
}

static cv::Mask renderGlyph(int kind, int W, float cx, float cy, float r, int th, float rot) {
    cv::Mask m(W, W);
    const float cs = std::cos(rot), sn = std::sin(rot);
    auto rp = [&](float lx, float ly) {
        return std::pair<float, float>{cx + (lx * cs - ly * sn), cy + (lx * sn + ly * cs)};
    };
    if (kind == Circle) {
        for (int a = 0; a < 48; ++a) {
            const float ang = 2.0f * 3.14159265f * a / 48.0f;
            stamp(m, cx + r * std::cos(ang), cy + r * std::sin(ang), th);
        }
    } else if (kind == Square) {
        const auto a = rp(-r, -r), b = rp(r, -r), c = rp(r, r), d = rp(-r, r);
        line(m, a.first, a.second, b.first, b.second, th);
        line(m, b.first, b.second, c.first, c.second, th);
        line(m, c.first, c.second, d.first, d.second, th);
        line(m, d.first, d.second, a.first, a.second, th);
    } else if (kind == Triangle) {
        const auto a = rp(0, -r), b = rp(-r, r), c = rp(r, r);
        line(m, a.first, a.second, b.first, b.second, th);
        line(m, b.first, b.second, c.first, c.second, th);
        line(m, c.first, c.second, a.first, a.second, th);
    } else { // Cross (a plus/X through the center)
        const auto a = rp(-r, 0), b = rp(r, 0), c = rp(0, -r), d = rp(0, r);
        line(m, a.first, a.second, b.first, b.second, th);
        line(m, c.first, c.second, d.first, d.second, th);
    }
    return m;
}

static cv::Mask renderVaried(int kind, int W, DeterministicRng& rng) {
    const float cx = W / 2.0f + rng.nextInt(-4, 4);
    const float cy = W / 2.0f + rng.nextInt(-4, 4);
    const float r = 10.0f + rng.nextInt(0, 6);
    const int th = 1 + rng.nextInt(0, 1);
    const float rot = (rng.nextFloat() - 0.5f) * 0.5f; // +/- ~0.25 rad
    return renderGlyph(kind, W, cx, cy, r, th, rot);
}

static float l2(const std::vector<float>& a, const std::vector<float>& b) {
    float s = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(s);
}

static void testNormalizationInvariance() {
    // The same shape drawn at different positions and sizes (same pen width)
    // normalizes to a similar feature vector - position/scale invariance - while a
    // clearly different shape is further away.
    const std::vector<float> circleA = ml::normalizeGlyph(renderGlyph(Circle, 48, 24, 24, 14, 1, 0), 12);
    const std::vector<float> circleB = ml::normalizeGlyph(renderGlyph(Circle, 48, 16, 30, 8, 1, 0), 12);
    const std::vector<float> cross = ml::normalizeGlyph(renderGlyph(Cross, 48, 24, 24, 13, 1, 0), 12);
    CHECK(l2(circleA, circleB) < l2(circleA, cross));
}

static void testRobustRecognitionAndLatency() {
    const int W = 48;
    const std::vector<std::string> labels = {"coin", "exit", "player", "enemy"}; // circle/square/triangle/cross
    const int classes = 4;

    // Training set: varied freehand-style glyphs.
    std::vector<std::vector<float>> X;
    std::vector<int> y;
    for (int k = 0; k < classes; ++k) {
        DeterministicRng rng(static_cast<std::uint64_t>(k) * 10007 + 1);
        for (int i = 0; i < 40; ++i) {
            X.push_back(ml::normalizeGlyph(renderVaried(k, W, rng), 12));
            y.push_back(k);
        }
    }
    ml::GlyphClassifier clf;
    clf.train(X, y, labels, /*epochs=*/400, /*lr=*/0.5f, /*seed=*/1);

    // Held-out set: different varied glyphs.
    int correct = 0, total = 0;
    double totalMicros = 0.0;
    for (int k = 0; k < classes; ++k) {
        DeterministicRng rng(static_cast<std::uint64_t>(k) * 10007 + 500000);
        for (int i = 0; i < 15; ++i) {
            const std::vector<float> f = ml::normalizeGlyph(renderVaried(k, W, rng), 12);
            const auto t0 = std::chrono::steady_clock::now();
            const ml::GlyphClassifier::Result r = clf.classify(f, 0.4f);
            const auto t1 = std::chrono::steady_clock::now();
            totalMicros += std::chrono::duration<double, std::micro>(t1 - t0).count();
            if (r.index == k) ++correct;
            ++total;
        }
    }
    const double accuracy = static_cast<double>(correct) / total;
    const double avgMicros = totalMicros / total;
    std::printf("[info] held-out accuracy %.2f (%d/%d), avg inference %.2f us\n", accuracy, correct,
                total, avgMicros);

    CHECK(accuracy >= 0.75);      // robust on varied hand drawings
    CHECK(avgMicros < 2000.0);    // well within an on-device latency budget
}

static void testLowConfidenceFallback() {
    const std::vector<std::string> labels = {"coin", "exit", "player", "enemy"};
    std::vector<std::vector<float>> X;
    std::vector<int> y;
    for (int k = 0; k < 4; ++k) {
        DeterministicRng rng(static_cast<std::uint64_t>(k) * 7 + 3);
        for (int i = 0; i < 20; ++i) {
            X.push_back(ml::normalizeGlyph(renderVaried(k, 48, rng), 12));
            y.push_back(k);
        }
    }
    ml::GlyphClassifier clf;
    clf.train(X, y, labels, 300, 0.5f, 2);

    // A blank patch has no ink: the classifier is not confident about any class.
    const cv::Mask blank(48, 48);
    const ml::GlyphClassifier::Result r = clf.classifyGlyph(blank, 12, 0.6f);
    CHECK(r.lowConfidence); // caller falls back to a default rather than trusting it
}

int main() {
    testNormalizationInvariance();
    testRobustRecognitionAndLatency();
    testLowConfidenceFallback();

    if (g_failures == 0) {
        std::printf("All glyph-net tests passed.\n");
        return 0;
    }
    std::printf("%d glyph-net test(s) failed.\n", g_failures);
    return 1;
}
