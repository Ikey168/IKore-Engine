#pragma once

#include "core/sim/Simulation.h" // DeterministicRng
#include "cv/Vectorize.h"        // cv::Mask

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

/**
 * @file GlyphNet.h
 * @brief On-device symbol classifier for freehand robustness (issue #173).
 *
 * Milestone 17. The shippable, portable inference core for symbol recognition
 * beyond the Tier-1 color scheme (#169): a normalization frontend that makes a
 * hand-drawn glyph invariant to where it was drawn, how big, and how thick, and a
 * small trained classifier that runs a fast forward pass on-device.
 *
 * normalizeGlyph average-pools an ink mask, cropped to its bounding box, into a
 * fixed-size density grid - so position, scale, and stroke-thickness variation are
 * removed before classification (lighting/skew are handled upstream by the
 * binarize/rectify stages). GlyphClassifier is a softmax classifier (a single
 * neural-net layer) trained by SGD; it is tiny and its inference is a handful of
 * dot products, well within a mobile latency budget. A deeper CNN can be trained
 * offline and dropped in behind the same normalize -> classify interface; the
 * classifier reports a confidence so low-confidence marks fall back gracefully.
 *
 * Pure std + the header-only Mask / RNG (no ML runtime), so it is unit-testable
 * headless. Header-only.
 */
namespace IKore {
namespace ml {

/// Average-pool an ink mask (cropped to its ink bounding box) into a size x size
/// density feature vector in [0, 1]. Empty masks yield an all-zero vector.
inline std::vector<float> normalizeGlyph(const cv::Mask& m, int size = 12) {
    std::vector<float> out(static_cast<std::size_t>(size) * size, 0.0f);
    int minX = m.width, minY = m.height, maxX = -1, maxY = -1;
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            if (m.get(x, y)) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < 0) return out; // no ink
    const float bw = static_cast<float>(maxX - minX + 1);
    const float bh = static_cast<float>(maxY - minY + 1);

    std::vector<float> sum(out.size(), 0.0f);
    std::vector<int> cnt(out.size(), 0);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            int ox = static_cast<int>((x - minX) * size / bw);
            int oy = static_cast<int>((y - minY) * size / bh);
            if (ox >= size) ox = size - 1;
            if (oy >= size) oy = size - 1;
            const std::size_t idx = static_cast<std::size_t>(oy) * size + ox;
            ++cnt[idx];
            sum[idx] += m.get(x, y) ? 1.0f : 0.0f;
        }
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = cnt[i] > 0 ? sum[i] / static_cast<float>(cnt[i]) : 0.0f;
    }
    return out;
}

/// A softmax (single-layer) classifier over normalized glyph features.
class GlyphClassifier {
public:
    struct Result {
        std::string label;
        int index{-1};
        float confidence{0.0f};
        bool lowConfidence{true};
    };

    /// Train on feature vectors @p X with integer labels @p y (SGD, cross-entropy).
    void train(const std::vector<std::vector<float>>& X, const std::vector<int>& y,
               const std::vector<std::string>& labels, int epochs = 300, float lr = 0.5f,
               std::uint64_t seed = 1) {
        m_labels = labels;
        m_numClasses = static_cast<int>(labels.size());
        m_featDim = X.empty() ? 0 : static_cast<int>(X[0].size());
        sim::DeterministicRng rng(seed);
        m_W.assign(static_cast<std::size_t>(m_numClasses) * m_featDim, 0.0f);
        m_b.assign(static_cast<std::size_t>(m_numClasses), 0.0f);
        for (float& w : m_W) w = (rng.nextFloat() - 0.5f) * 0.01f;

        const int n = static_cast<int>(X.size());
        std::vector<int> order(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) order[static_cast<std::size_t>(i)] = i;

        for (int e = 0; e < epochs; ++e) {
            for (int i = n - 1; i > 0; --i) { // Fisher-Yates shuffle
                const int j = rng.nextInt(0, i);
                std::swap(order[static_cast<std::size_t>(i)], order[static_cast<std::size_t>(j)]);
            }
            for (int t : order) {
                const std::vector<float>& x = X[static_cast<std::size_t>(t)];
                const int yt = y[static_cast<std::size_t>(t)];
                std::vector<float> p = softmaxLogits(x);
                for (int c = 0; c < m_numClasses; ++c) {
                    const float g = p[static_cast<std::size_t>(c)] - (c == yt ? 1.0f : 0.0f);
                    m_b[static_cast<std::size_t>(c)] -= lr * g;
                    const float lg = lr * g;
                    float* wc = &m_W[static_cast<std::size_t>(c) * m_featDim];
                    for (int k = 0; k < m_featDim; ++k) wc[k] -= lg * x[static_cast<std::size_t>(k)];
                }
            }
        }
    }

    /// Class probabilities for a feature vector (softmax over the linear scores).
    std::vector<float> predictProba(const std::vector<float>& x) const { return softmaxLogits(x); }

    /// Classify a normalized feature vector; lowConfidence when below @p minConfidence.
    Result classify(const std::vector<float>& features, float minConfidence = 0.5f) const {
        Result r;
        if (m_numClasses == 0) return r;
        const std::vector<float> p = softmaxLogits(features);
        int best = 0;
        for (int c = 1; c < m_numClasses; ++c) {
            if (p[static_cast<std::size_t>(c)] > p[static_cast<std::size_t>(best)]) best = c;
        }
        r.index = best;
        r.label = m_labels[static_cast<std::size_t>(best)];
        r.confidence = p[static_cast<std::size_t>(best)];
        r.lowConfidence = r.confidence < minConfidence;
        return r;
    }

    /// Convenience: normalize an ink mask and classify it.
    Result classifyGlyph(const cv::Mask& mask, int size = 12, float minConfidence = 0.5f) const {
        return classify(normalizeGlyph(mask, size), minConfidence);
    }

    int numClasses() const { return m_numClasses; }
    int featureDim() const { return m_featDim; }
    const std::vector<std::string>& labels() const { return m_labels; }

    /// Export the trained model to a portable, whitespace-separated text blob so it
    /// can ship on-device and be reloaded with deserialize() (issue #240).
    std::string serialize() const {
        std::ostringstream os;
        os << "GLYPHNET1\n" << m_numClasses << ' ' << m_featDim << '\n';
        for (const std::string& l : m_labels) os << l << '\n';
        os << std::scientific << std::setprecision(9);
        for (int c = 0; c < m_numClasses; ++c) os << m_b[static_cast<std::size_t>(c)] << '\n';
        for (float w : m_W) os << w << '\n';
        return os.str();
    }

    /// Load a model previously produced by serialize(). Returns false (leaving the
    /// classifier unchanged) on a malformed or truncated blob, so callers can fall
    /// back to the heuristic path when a model is unavailable.
    bool deserialize(const std::string& blob) {
        std::istringstream is(blob);
        std::string magic;
        if (!(is >> magic) || magic != "GLYPHNET1") return false;
        int K = 0, D = 0;
        if (!(is >> K >> D) || K <= 0 || D <= 0) return false;
        std::vector<std::string> labels(static_cast<std::size_t>(K));
        for (int c = 0; c < K; ++c) {
            if (!(is >> labels[static_cast<std::size_t>(c)])) return false;
        }
        std::vector<float> b(static_cast<std::size_t>(K), 0.0f);
        for (int c = 0; c < K; ++c) {
            if (!(is >> b[static_cast<std::size_t>(c)])) return false;
        }
        std::vector<float> W(static_cast<std::size_t>(K) * D, 0.0f);
        for (std::size_t i = 0; i < W.size(); ++i) {
            if (!(is >> W[i])) return false;
        }
        m_numClasses = K;
        m_featDim = D;
        m_labels = std::move(labels);
        m_b = std::move(b);
        m_W = std::move(W);
        return true;
    }

private:
    std::vector<float> softmaxLogits(const std::vector<float>& x) const {
        std::vector<float> logit(static_cast<std::size_t>(m_numClasses), 0.0f);
        for (int c = 0; c < m_numClasses; ++c) {
            float s = m_b[static_cast<std::size_t>(c)];
            const float* wc = &m_W[static_cast<std::size_t>(c) * m_featDim];
            const int k = std::min(m_featDim, static_cast<int>(x.size()));
            for (int i = 0; i < k; ++i) s += wc[i] * x[static_cast<std::size_t>(i)];
            logit[static_cast<std::size_t>(c)] = s;
        }
        float mx = logit[0];
        for (float v : logit) mx = std::max(mx, v);
        float z = 0.0f;
        for (float& v : logit) {
            v = std::exp(v - mx);
            z += v;
        }
        if (z > 0.0f) {
            for (float& v : logit) v /= z;
        }
        return logit;
    }

    std::vector<std::string> m_labels;
    std::vector<float> m_W;
    std::vector<float> m_b;
    int m_numClasses{0};
    int m_featDim{0};
};

} // namespace ml
} // namespace IKore
