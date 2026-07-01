#pragma once

#include <cstddef>
#include <deque>
#include <vector>

#if defined(__linux__)
#include <fstream>
#include <unistd.h>
#endif

/**
 * @file PerfStats.h
 * @brief Rolling frame-time / FPS / memory stats for the perf overlay (issue #53).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer-agnostic core
 * behind the ImGui performance overlay: record a frame time each frame and read
 * back the instantaneous and averaged FPS / frame time, min/max, a rolling history
 * for the graph, and resident memory. The ImGui panel in DebugUI draws these
 * values; keeping the aggregation here lets it be unit-tested without a GL context.
 *
 * record() is a cheap deque push (no allocation once warmed, no I/O), so it costs
 * nothing measurable when the overlay is hidden. Memory is only sampled on
 * refreshMemory(), which the overlay calls while visible.
 *
 * Header-only, std only (plus /proc on Linux for RSS).
 */
namespace IKore {

class PerfStats {
public:
    explicit PerfStats(std::size_t historyCapacity = 120)
        : m_capacity(historyCapacity == 0 ? 1 : historyCapacity) {}

    /// Record one frame's duration (seconds).
    void record(double frameSeconds) {
        const float ms = static_cast<float>(frameSeconds * 1000.0);
        m_history.push_back(ms);
        while (m_history.size() > m_capacity) m_history.pop_front();
        m_last = ms;
        ++m_count;
    }

    float lastFrameMs() const { return m_last; }
    double fps() const { return m_last > 0.0f ? 1000.0 / m_last : 0.0; }

    float avgFrameMs() const {
        if (m_history.empty()) return 0.0f;
        float sum = 0.0f;
        for (float v : m_history) sum += v;
        return sum / static_cast<float>(m_history.size());
    }
    double avgFps() const {
        const float a = avgFrameMs();
        return a > 0.0f ? 1000.0 / a : 0.0;
    }

    float minFrameMs() const {
        if (m_history.empty()) return 0.0f;
        float m = m_history.front();
        for (float v : m_history) {
            if (v < m) m = v;
        }
        return m;
    }
    float maxFrameMs() const {
        if (m_history.empty()) return 0.0f;
        float m = m_history.front();
        for (float v : m_history) {
            if (v > m) m = v;
        }
        return m;
    }

    std::size_t frameCount() const { return m_count; }
    std::size_t historySize() const { return m_history.size(); }
    std::size_t capacity() const { return m_capacity; }

    /// Rolling frame-time history (ms), oldest first - contiguous for a graph.
    std::vector<float> historyMs() const { return std::vector<float>(m_history.begin(), m_history.end()); }

    void reset() {
        m_history.clear();
        m_last = 0.0f;
        m_count = 0;
        m_memoryBytes = 0;
    }

    /// Last sampled resident set size in bytes (0 if never sampled / unsupported).
    long memoryBytes() const { return m_memoryBytes; }
    /// Sample resident memory now (called by the overlay while visible).
    void refreshMemory() { m_memoryBytes = residentBytes(); }

    /// Current process resident set size in bytes (0 where unsupported).
    static long residentBytes() {
#if defined(__linux__)
        std::ifstream statm("/proc/self/statm");
        long totalPages = 0, residentPages = 0;
        if (statm >> totalPages >> residentPages) {
            return residentPages * sysconf(_SC_PAGESIZE);
        }
        return 0;
#else
        return 0;
#endif
    }

private:
    std::deque<float> m_history;
    std::size_t m_capacity;
    float m_last{0.0f};
    std::size_t m_count{0};
    long m_memoryBytes{0};
};

} // namespace IKore
