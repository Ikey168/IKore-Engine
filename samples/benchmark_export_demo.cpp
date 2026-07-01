// Sample: capture performance metrics over a run and export them.
//
// Feeds a simulated frame loop into the benchmark recorder (the same core the
// in-engine benchmark panel uses), prints the summary, and exports the per-frame
// samples via the deterministic data-export format as CSV (to a file) and JSON
// (to stdout). No GL context required.
//
// Build (also wired into CMake as sample_benchmark_export):
//   g++ -std=c++17 -I src samples/benchmark_export_demo.cpp -o sample_benchmark_export
//   ./sample_benchmark_export

#include "core/Benchmark.h"

#include <cstdio>
#include <iostream>

using namespace IKore;

int main() {
    Benchmark benchmark;
    benchmark.start();

    // Simulate 120 frames at ~60 FPS, with an occasional hitch and slowly growing
    // memory, so the exported data has some shape to inspect.
    for (int frame = 0; frame < 120; ++frame) {
        const double frameSeconds = (frame % 20 == 0) ? 0.033 : 0.016; // periodic hitch
        const long memoryBytes = 50L * 1024 * 1024 + static_cast<long>(frame) * 4096;
        benchmark.record(frameSeconds, memoryBytes);
    }
    benchmark.stop();

    std::printf("Captured %zu frames over %.3f s.\n", benchmark.sampleCount(),
                benchmark.durationSeconds());
    std::printf("FPS: avg %.1f (min %.1f, max %.1f)\n", benchmark.avgFps(), benchmark.minFps(),
                benchmark.maxFps());
    std::printf("Frame ms: avg %.3f (min %.3f, max %.3f)\n", benchmark.avgFrameMs(),
                benchmark.minFrameMs(), benchmark.maxFrameMs());
    std::printf("Peak memory: %.1f MB\n",
                static_cast<double>(benchmark.peakMemoryBytes()) / (1024.0 * 1024.0));

    const char* csvPath = "benchmark_sample.csv";
    if (benchmark.exportToFile(csvPath, sim::ExportFormat::Csv)) {
        std::printf("Exported %zu samples to %s (CSV).\n", benchmark.sampleCount(), csvPath);
    } else {
        std::printf("Failed to write %s.\n", csvPath);
    }

    std::printf("\nFirst rows as JSON:\n");
    Benchmark preview;
    preview.start();
    preview.record(0.016, 52428800);
    preview.record(0.033, 52432896);
    preview.stop();
    preview.exportTo(std::cout, sim::ExportFormat::Json);

    return 0;
}
