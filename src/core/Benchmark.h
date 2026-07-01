#pragma once

#include "core/sim/DataExport.h"

#include <cstddef>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

/**
 * @file Benchmark.h
 * @brief Performance benchmark recorder with data export (issue #62).
 *
 * Milestone 9 (In-Engine Editor & Tooling). Captures FPS, frame time, and memory
 * over a run and exports the samples to a structured, parseable file. It reuses the
 * same frame-time values the perf overlay uses (PerfStats, #53) - the engine feeds
 * each frame's duration and resident memory - and the deterministic data-export
 * format (DataExporter, #155) for the exported CSV/JSON.
 *
 * Capture is explicit: record() is a no-op on a single branch unless a run is
 * active, so it costs nothing when idle or disabled. Header-only, std only.
 */
namespace IKore {

struct BenchmarkSample {
    double timeSeconds{0.0}; ///< Accumulated time since the run started.
    double frameMs{0.0};
    double fps{0.0};
    long memoryBytes{0};
};

class Benchmark {
public:
    /// Begin a fresh run: clears prior samples and starts capturing.
    void start() {
        reset();
        m_capturing = true;
    }

    /// Stop capturing (samples are kept for inspection/export).
    void stop() { m_capturing = false; }

    /// Clear all samples and state.
    void reset() {
        m_samples.clear();
        m_elapsed = 0.0;
        m_peakMemory = 0;
        m_capturing = false;
    }

    bool capturing() const { return m_capturing; }

    /**
     * @brief Record one frame while capturing; a no-op otherwise.
     * @param frameSeconds This frame's duration in seconds.
     * @param memoryBytes  Resident memory this frame (e.g. PerfStats::residentBytes()).
     */
    void record(double frameSeconds, long memoryBytes = 0) {
        if (!m_capturing) return;
        m_elapsed += frameSeconds;
        const double ms = frameSeconds * 1000.0;
        BenchmarkSample sample;
        sample.timeSeconds = m_elapsed;
        sample.frameMs = ms;
        sample.fps = ms > 0.0 ? 1000.0 / ms : 0.0;
        sample.memoryBytes = memoryBytes;
        m_samples.push_back(sample);
        if (memoryBytes > m_peakMemory) m_peakMemory = memoryBytes;
    }

    std::size_t sampleCount() const { return m_samples.size(); }
    const std::vector<BenchmarkSample>& samples() const { return m_samples; }

    double durationSeconds() const { return m_elapsed; }

    double avgFrameMs() const {
        if (m_samples.empty()) return 0.0;
        double sum = 0.0;
        for (const BenchmarkSample& s : m_samples) sum += s.frameMs;
        return sum / static_cast<double>(m_samples.size());
    }
    double minFrameMs() const {
        if (m_samples.empty()) return 0.0;
        double m = m_samples.front().frameMs;
        for (const BenchmarkSample& s : m_samples) {
            if (s.frameMs < m) m = s.frameMs;
        }
        return m;
    }
    double maxFrameMs() const {
        if (m_samples.empty()) return 0.0;
        double m = m_samples.front().frameMs;
        for (const BenchmarkSample& s : m_samples) {
            if (s.frameMs > m) m = s.frameMs;
        }
        return m;
    }

    double avgFps() const {
        const double a = avgFrameMs();
        return a > 0.0 ? 1000.0 / a : 0.0;
    }
    double minFps() const {
        const double m = maxFrameMs(); // slowest frame -> lowest FPS
        return m > 0.0 ? 1000.0 / m : 0.0;
    }
    double maxFps() const {
        const double m = minFrameMs(); // fastest frame -> highest FPS
        return m > 0.0 ? 1000.0 / m : 0.0;
    }

    long peakMemoryBytes() const { return m_peakMemory; }

    /// Export all samples via the deterministic DataExporter format (#155).
    void exportTo(std::ostream& out, sim::ExportFormat format = sim::ExportFormat::Csv) const {
        sim::DataExporter exporter;
        exporter.begin(out, format, {"frame", "time_s", "frame_ms", "fps", "memory_bytes"});
        for (std::size_t i = 0; i < m_samples.size(); ++i) {
            const BenchmarkSample& s = m_samples[i];
            exporter.record({static_cast<double>(i), s.timeSeconds, s.frameMs, s.fps,
                             static_cast<double>(s.memoryBytes)});
        }
        exporter.end();
    }

    /// Export to @p path. Returns false if the file cannot be opened.
    bool exportToFile(const std::string& path, sim::ExportFormat format = sim::ExportFormat::Csv) const {
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        exportTo(out, format);
        return static_cast<bool>(out);
    }

private:
    std::vector<BenchmarkSample> m_samples;
    double m_elapsed{0.0};
    long m_peakMemory{0};
    bool m_capturing{false};
};

} // namespace IKore
