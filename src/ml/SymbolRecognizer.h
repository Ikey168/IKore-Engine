#pragma once

#include "core/sim/Simulation.h" // sim::DeterministicRng
#include "cv/Vectorize.h"        // cv::Mask
#include "ml/GlyphNet.h"         // normalizeGlyph, GlyphClassifier

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file SymbolRecognizer.h
 * @brief Phase 4 symbol-recognition maturity: data pipeline, model export, an
 *        accuracy/confusion harness, and a heuristic fallback (issue #240).
 *
 * Milestone 16/17, "Doodlebound". GlyphNet.h provides the invariant feature
 * frontend (normalizeGlyph) and a trainable softmax model (GlyphClassifier). This
 * header matures that into the shippable recognizer for the seven game symbols and
 * the reproducible pipeline around it, which PHONE_GAME_CONCEPT.md 5 (step 5) calls
 * the long-term data moat:
 *
 *   - A deterministic synthetic doodle generator (renderSymbol + makeDataset): draws
 *     each symbol as a freehand-varied ink mask - jittered position, size, stroke
 *     width, rotation, and per-vertex wobble - seeded so a given seed always yields
 *     the same data (the "no nondeterminism in the test path" requirement).
 *   - Model export/import lives on GlyphClassifier (serialize/deserialize).
 *   - HeuristicClassifier: a nearest-centroid template matcher built from canonical
 *     renders, needing no training, as the offline fallback when no model is loaded.
 *   - ConfusionMatrix + evaluate(): a labeled-holdout accuracy harness with per-class
 *     recall/precision so weak symbols are visible; tests gate on a minimum accuracy.
 *   - SymbolRecognizer: the facade that uses the model when present and confident and
 *     otherwise falls back to the heuristic.
 *
 * Pure std + the header-only Mask / RNG (no ML runtime), unit-testable headless.
 */
namespace IKore {
namespace ml {

/// The seven game symbols the recognizer classifies. Ordering is the label index.
enum class Symbol { Start = 0, Exit, Enemy, Coin, Plus, KeyDoor, Hazard };

inline const std::vector<std::string>& symbolLabels() {
    static const std::vector<std::string> kLabels = {
        "start", "exit", "enemy", "coin", "plus", "keydoor", "hazard"};
    return kLabels;
}
inline int symbolCount() { return static_cast<int>(symbolLabels().size()); }

// ---------------------------------------------------------------------------
// Deterministic synthetic doodle generator (the training-data pipeline)
// ---------------------------------------------------------------------------
namespace detail {

inline void stampDisk(cv::Mask& m, int cx, int cy, int th) {
    if (th < 0) th = 0;
    for (int dy = -th; dy <= th; ++dy) {
        for (int dx = -th; dx <= th; ++dx) {
            if (dx * dx + dy * dy <= th * th + 1) m.set(cx + dx, cy + dy, 1);
        }
    }
}

inline void drawLine(cv::Mask& m, float x0, float y0, float x1, float y1, int th) {
    int ix0 = static_cast<int>(std::lround(x0)), iy0 = static_cast<int>(std::lround(y0));
    const int ix1 = static_cast<int>(std::lround(x1)), iy1 = static_cast<int>(std::lround(y1));
    const int dx = std::abs(ix1 - ix0), dy = -std::abs(iy1 - iy0);
    const int sx = ix0 < ix1 ? 1 : -1, sy = iy0 < iy1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        stampDisk(m, ix0, iy0, th);
        if (ix0 == ix1 && iy0 == iy1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += sx; }
        if (e2 <= dx) { err += dx; iy0 += sy; }
    }
}

inline void drawCircle(cv::Mask& m, float cx, float cy, float r, int th, float wobble,
                       sim::DeterministicRng& rng) {
    const int steps = 56;
    for (int a = 0; a < steps; ++a) {
        const float ang = 2.0f * 3.14159265358979f * a / steps;
        const float rr = r + (wobble > 0.0f ? (rng.nextFloat() - 0.5f) * wobble : 0.0f);
        stampDisk(m, static_cast<int>(std::lround(cx + rr * std::cos(ang))),
                  static_cast<int>(std::lround(cy + rr * std::sin(ang))), th);
    }
}

} // namespace detail

/// Freehand-variation parameters for one rendered glyph.
struct RenderParams {
    float cx{0}, cy{0}, r{10};
    int th{1};
    float rot{0};
    float wobble{0};
};

/// Draw @p sym into a WxW ink mask with the given freehand variation. @p rng drives
/// per-vertex/point wobble so the same seed reproduces the same drawing.
inline cv::Mask renderSymbol(Symbol sym, int W, const RenderParams& p, sim::DeterministicRng& rng) {
    cv::Mask m(W, W);
    const float cs = std::cos(p.rot), sn = std::sin(p.rot);
    const float w = p.wobble;
    auto jit = [&]() { return w > 0.0f ? (rng.nextFloat() - 0.5f) * w : 0.0f; };
    // Local (rotated, wobbled) point -> image point.
    auto pt = [&](float lx, float ly) {
        const float rx = lx * cs - ly * sn, ry = lx * sn + ly * cs;
        return std::pair<float, float>{p.cx + rx + jit(), p.cy + ry + jit()};
    };
    auto seg = [&](std::pair<float, float> a, std::pair<float, float> b) {
        detail::drawLine(m, a.first, a.second, b.first, b.second, p.th);
    };
    const float r = p.r;
    switch (sym) {
        case Symbol::Coin: {
            detail::drawCircle(m, p.cx, p.cy, r, p.th, w, rng);
            break;
        }
        case Symbol::Plus: {
            seg(pt(-r, 0), pt(r, 0));
            seg(pt(0, -r), pt(0, r));
            break;
        }
        case Symbol::Enemy: { // an X (diagonals corner to corner)
            seg(pt(-r, -r), pt(r, r));
            seg(pt(-r, r), pt(r, -r));
            break;
        }
        case Symbol::Exit: { // a box outline (a doorway)
            const auto a = pt(-r, -r), b = pt(r, -r), c = pt(r, r), d = pt(-r, r);
            seg(a, b); seg(b, c); seg(c, d); seg(d, a);
            break;
        }
        case Symbol::Start: { // a triangle (a play marker)
            const auto a = pt(0, -r), b = pt(-r, r), c = pt(r, r);
            seg(a, b); seg(b, c); seg(c, a);
            break;
        }
        case Symbol::Hazard: { // a diamond (edge-midpoint to edge-midpoint)
            const auto a = pt(0, -r), b = pt(r, 0), c = pt(0, r), d = pt(-r, 0);
            seg(a, b); seg(b, c); seg(c, d); seg(d, a);
            break;
        }
        case Symbol::KeyDoor: { // a key: a small ring head over a vertical shaft
            const std::pair<float, float> head = pt(0, -r * 0.5f);
            detail::drawCircle(m, head.first, head.second, r * 0.5f, p.th, w, rng);
            seg(pt(0, 0), pt(0, r));
            break;
        }
    }
    return m;
}

/// Sample freehand-variation parameters for a WxW canvas from @p rng.
inline RenderParams sampleParams(int W, sim::DeterministicRng& rng) {
    RenderParams p;
    p.cx = W / 2.0f + rng.nextInt(-5, 5);
    p.cy = W / 2.0f + rng.nextInt(-5, 5);
    p.r = 9.0f + rng.nextInt(0, 8);
    p.th = 1 + rng.nextInt(0, 1);
    p.rot = (rng.nextFloat() - 0.5f) * 0.7f; // +/- ~0.35 rad, real hand tilt
    p.wobble = rng.nextFloat() * 2.6f;        // freehand tremor
    return p;
}

/// A labeled feature dataset.
struct Dataset {
    std::vector<std::vector<float>> X;
    std::vector<int> y;
    std::vector<std::string> labels;
    int size() const { return static_cast<int>(X.size()); }
};

/// Build a deterministic dataset of @p perClass freehand samples per symbol.
/// The same @p seed and @p perClass always produce identical data.
inline Dataset makeDataset(std::uint64_t seed, int perClass, int W = 48, int featSize = 12) {
    Dataset ds;
    ds.labels = symbolLabels();
    for (int k = 0; k < symbolCount(); ++k) {
        sim::DeterministicRng rng(seed + static_cast<std::uint64_t>(k) * 1000003u);
        for (int i = 0; i < perClass; ++i) {
            const RenderParams p = sampleParams(W, rng);
            cv::Mask mask = renderSymbol(static_cast<Symbol>(k), W, p, rng);
            ds.X.push_back(normalizeGlyph(mask, featSize));
            ds.y.push_back(k);
        }
    }
    return ds;
}

// ---------------------------------------------------------------------------
// Heuristic fallback: nearest-centroid template matching
// ---------------------------------------------------------------------------

/// A training-free classifier: assigns a glyph to the class whose average canonical
/// feature template it is closest to. Used when no trained model is available.
class HeuristicClassifier {
public:
    struct Result {
        int index{-1};
        std::string label;
        float confidence{0.0f};
        bool lowConfidence{true};
    };

    /// Build class templates by averaging @p perClass low-noise canonical renders.
    void fitCanonical(int perClass = 24, int W = 48, int featSize = 12, std::uint64_t seed = 777) {
        m_labels = symbolLabels();
        m_featDim = featSize * featSize;
        m_centroids.assign(symbolLabels().size(), std::vector<float>(static_cast<std::size_t>(m_featDim), 0.0f));
        for (int k = 0; k < symbolCount(); ++k) {
            sim::DeterministicRng rng(seed + static_cast<std::uint64_t>(k) * 7919u);
            std::vector<float>& cen = m_centroids[static_cast<std::size_t>(k)];
            for (int i = 0; i < perClass; ++i) {
                RenderParams p = sampleParams(W, rng);
                p.wobble *= 0.4f; // canonical: less wobble than the training corpus
                const std::vector<float> f = normalizeGlyph(renderSymbol(static_cast<Symbol>(k), W, p, rng), featSize);
                for (int d = 0; d < m_featDim; ++d) cen[static_cast<std::size_t>(d)] += f[static_cast<std::size_t>(d)];
            }
            for (float& v : cen) v /= static_cast<float>(std::max(1, perClass));
        }
    }

    Result classify(const std::vector<float>& features, float minConfidence = 0.5f) const {
        Result r;
        if (m_centroids.empty()) return r;
        // Distance to every centroid; convert to a softmax-style confidence.
        std::vector<float> dist(m_centroids.size(), 0.0f);
        float best = 1e30f, second = 1e30f;
        int bestIdx = 0;
        for (std::size_t c = 0; c < m_centroids.size(); ++c) {
            float d = 0.0f;
            const std::vector<float>& cen = m_centroids[c];
            const int n = std::min(m_featDim, static_cast<int>(features.size()));
            for (int i = 0; i < n; ++i) {
                const float e = features[static_cast<std::size_t>(i)] - cen[static_cast<std::size_t>(i)];
                d += e * e;
            }
            dist[c] = d;
            if (d < best) { second = best; best = d; bestIdx = static_cast<int>(c); }
            else if (d < second) { second = d; }
        }
        r.index = bestIdx;
        r.label = m_labels[static_cast<std::size_t>(bestIdx)];
        // Confidence from the separation between the best and runner-up distances.
        const float denom = best + second;
        r.confidence = denom > 1e-9f ? (second - best) / denom + 0.5f : 0.5f;
        if (r.confidence > 1.0f) r.confidence = 1.0f;
        r.lowConfidence = r.confidence < minConfidence;
        return r;
    }

    Result classifyGlyph(const cv::Mask& mask, int featSize = 12, float minConfidence = 0.5f) const {
        return classify(normalizeGlyph(mask, featSize), minConfidence);
    }

    bool ready() const { return !m_centroids.empty(); }

private:
    std::vector<std::vector<float>> m_centroids;
    std::vector<std::string> m_labels;
    int m_featDim{0};
};

// ---------------------------------------------------------------------------
// Accuracy harness + confusion matrix
// ---------------------------------------------------------------------------

/// A KxK confusion matrix (rows = true class, cols = predicted class) with
/// per-class recall/precision and a printable report so weak symbols are visible.
struct ConfusionMatrix {
    std::vector<std::string> labels;
    std::vector<std::vector<int>> counts; // counts[true][pred]

    explicit ConfusionMatrix(const std::vector<std::string>& l = {})
        : labels(l), counts(l.size(), std::vector<int>(l.size(), 0)) {}

    void add(int trueClass, int predClass) {
        if (trueClass < 0 || predClass < 0) return;
        if (trueClass >= static_cast<int>(counts.size()) || predClass >= static_cast<int>(counts.size())) return;
        ++counts[static_cast<std::size_t>(trueClass)][static_cast<std::size_t>(predClass)];
    }

    int total() const {
        int n = 0;
        for (const std::vector<int>& row : counts) {
            for (int v : row) n += v;
        }
        return n;
    }
    int correct() const {
        int n = 0;
        for (std::size_t i = 0; i < counts.size(); ++i) n += counts[i][i];
        return n;
    }
    double accuracy() const {
        const int t = total();
        return t > 0 ? static_cast<double>(correct()) / t : 0.0;
    }
    /// Fraction of true-class @p c examples predicted correctly.
    double recall(int c) const {
        int row = 0;
        for (int v : counts[static_cast<std::size_t>(c)]) row += v;
        return row > 0 ? static_cast<double>(counts[static_cast<std::size_t>(c)][static_cast<std::size_t>(c)]) / row : 0.0;
    }
    /// Fraction of predicted-class @p c examples that were correct.
    double precision(int c) const {
        int col = 0;
        for (std::size_t r = 0; r < counts.size(); ++r) col += counts[r][static_cast<std::size_t>(c)];
        return col > 0 ? static_cast<double>(counts[static_cast<std::size_t>(c)][static_cast<std::size_t>(c)]) / col : 0.0;
    }
    /// Lowest per-class recall (the weakest symbol) and its class index.
    double minRecall(int* worstClass = nullptr) const {
        double worst = 2.0; // above any real recall so the first class always wins
        int idx = -1;
        for (std::size_t c = 0; c < counts.size(); ++c) {
            const double rc = recall(static_cast<int>(c));
            if (rc < worst) { worst = rc; idx = static_cast<int>(c); }
        }
        if (worstClass) *worstClass = idx;
        return idx < 0 ? 0.0 : worst;
    }

    std::string report() const {
        std::string s = "confusion (rows=true, cols=pred):\n";
        for (std::size_t r = 0; r < counts.size(); ++r) {
            s += labels[r];
            s += ": ";
            for (std::size_t p = 0; p < counts[r].size(); ++p) {
                s += std::to_string(counts[r][p]);
                s += ' ';
            }
            s += "| recall=";
            const int pct = static_cast<int>(std::lround(recall(static_cast<int>(r)) * 100.0));
            s += std::to_string(pct);
            s += "%\n";
        }
        return s;
    }
};

/// Evaluate a trained model on a labeled dataset into a confusion matrix.
inline ConfusionMatrix evaluate(const GlyphClassifier& clf, const Dataset& holdout) {
    ConfusionMatrix cm(holdout.labels);
    for (int i = 0; i < holdout.size(); ++i) {
        const GlyphClassifier::Result r = clf.classify(holdout.X[static_cast<std::size_t>(i)], 0.0f);
        cm.add(holdout.y[static_cast<std::size_t>(i)], r.index);
    }
    return cm;
}

/// Evaluate the heuristic fallback on a labeled dataset into a confusion matrix.
inline ConfusionMatrix evaluateHeuristic(const HeuristicClassifier& clf, const Dataset& holdout) {
    ConfusionMatrix cm(holdout.labels);
    for (int i = 0; i < holdout.size(); ++i) {
        const HeuristicClassifier::Result r = clf.classify(holdout.X[static_cast<std::size_t>(i)], 0.0f);
        cm.add(holdout.y[static_cast<std::size_t>(i)], r.index);
    }
    return cm;
}

// ---------------------------------------------------------------------------
// Recognizer facade (model with heuristic fallback)
// ---------------------------------------------------------------------------

/// Recognizes a symbol using the trained model when it is present and confident,
/// and otherwise the training-free heuristic. The heuristic is always available so
/// the recognizer never fails to produce a guess.
class SymbolRecognizer {
public:
    struct Prediction {
        int index{-1};
        std::string label;
        float confidence{0.0f};
        bool lowConfidence{true};
        bool usedFallback{false};
    };

    SymbolRecognizer() { m_fallback.fitCanonical(); }

    /// Install a trained model directly.
    void setModel(const GlyphClassifier& model) {
        m_model = model;
        m_hasModel = m_model.numClasses() > 0;
    }
    /// Load a model exported with GlyphClassifier::serialize(). Returns false and
    /// leaves the recognizer on the heuristic path if the blob is unusable.
    bool loadModel(const std::string& blob) {
        GlyphClassifier m;
        if (!m.deserialize(blob)) {
            m_hasModel = false;
            return false;
        }
        m_model = m;
        m_hasModel = true;
        return true;
    }
    bool hasModel() const { return m_hasModel; }

    Prediction recognize(const cv::Mask& mask, int featSize = 12, float minConfidence = 0.5f) const {
        const std::vector<float> f = normalizeGlyph(mask, featSize);
        Prediction out;
        if (m_hasModel) {
            const GlyphClassifier::Result r = m_model.classify(f, minConfidence);
            if (!r.lowConfidence) {
                out.index = r.index;
                out.label = r.label;
                out.confidence = r.confidence;
                out.lowConfidence = false;
                out.usedFallback = false;
                return out;
            }
        }
        // No model, or the model was not confident: use the heuristic fallback.
        const HeuristicClassifier::Result h = m_fallback.classify(f, minConfidence);
        out.index = h.index;
        out.label = h.label;
        out.confidence = h.confidence;
        out.lowConfidence = h.lowConfidence;
        out.usedFallback = true;
        return out;
    }

private:
    GlyphClassifier m_model;
    HeuristicClassifier m_fallback;
    bool m_hasModel{false};
};

} // namespace ml
} // namespace IKore
