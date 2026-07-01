// Test for the performance benchmark recorder - Milestone 9, #62.
//
// Verifies capture lifecycle (only records while running), sample values, summary
// statistics (avg/min/max frame time and FPS, peak memory, duration), and export
// through the deterministic DataExporter format (#155) as CSV and JSON, including
// a round trip to disk.
//
// Pure std + header-only (reuses the header-only DataExporter):
//   g++ -std=c++17 -I src tests/test_benchmark.cpp -o test_benchmark

#include "core/Benchmark.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(double a, double b) { return std::fabs(a - b) < 1e-4; }

static std::size_t countLines(const std::string& s) {
    std::size_t n = 0;
    for (char c : s) {
        if (c == '\n') ++n;
    }
    return n;
}

static void testCaptureLifecycle() {
    Benchmark b;
    CHECK(!b.capturing());

    // Records before start are ignored (negligible cost when idle/disabled).
    b.record(0.016, 100);
    CHECK(b.sampleCount() == 0);

    b.start();
    CHECK(b.capturing());
    b.record(0.010, 1000);
    b.record(0.020, 2000);
    CHECK(b.sampleCount() == 2);

    b.stop();
    CHECK(!b.capturing());
    b.record(0.030, 3000); // ignored after stop
    CHECK(b.sampleCount() == 2);

    // start() clears the prior run.
    b.start();
    CHECK(b.sampleCount() == 0);
    CHECK(approx(b.durationSeconds(), 0.0));
}

static void testSamplesAndSummary() {
    Benchmark b;
    b.start();
    b.record(0.010, 1000); // 10 ms -> 100 fps
    b.record(0.020, 2000); // 20 ms -> 50 fps
    b.record(0.030, 1500); // 30 ms -> ~33.3 fps
    b.stop();

    CHECK(b.sampleCount() == 3);

    // Time accumulates across samples.
    CHECK(approx(b.samples()[0].timeSeconds, 0.01));
    CHECK(approx(b.samples()[1].timeSeconds, 0.03));
    CHECK(approx(b.samples()[2].timeSeconds, 0.06));
    CHECK(approx(b.durationSeconds(), 0.06));

    // Per-sample values.
    CHECK(approx(b.samples()[0].frameMs, 10.0));
    CHECK(approx(b.samples()[0].fps, 100.0));
    CHECK(b.samples()[1].memoryBytes == 2000);

    // Summary.
    CHECK(approx(b.avgFrameMs(), 20.0));
    CHECK(approx(b.minFrameMs(), 10.0));
    CHECK(approx(b.maxFrameMs(), 30.0));
    CHECK(approx(b.avgFps(), 50.0));                 // 1000 / 20
    CHECK(approx(b.maxFps(), 100.0));                // fastest frame (10 ms)
    CHECK(approx(b.minFps(), 1000.0 / 30.0));        // slowest frame (30 ms)
    CHECK(b.peakMemoryBytes() == 2000);
}

static void testEmptySummarySafe() {
    Benchmark b;
    CHECK(approx(b.avgFrameMs(), 0.0));
    CHECK(approx(b.minFrameMs(), 0.0));
    CHECK(approx(b.maxFrameMs(), 0.0));
    CHECK(approx(b.avgFps(), 0.0));
    CHECK(approx(b.minFps(), 0.0));
    CHECK(approx(b.maxFps(), 0.0));
    CHECK(b.peakMemoryBytes() == 0);
    CHECK(approx(b.durationSeconds(), 0.0));
}

static void testCsvExport() {
    Benchmark b;
    b.start();
    b.record(0.010, 1000);
    b.record(0.020, 2000);
    b.stop();

    std::ostringstream os;
    b.exportTo(os, sim::ExportFormat::Csv);
    const std::string out = os.str();

    // Header + one row per sample, and clean integral formatting.
    CHECK(out.find("frame,time_s,frame_ms,fps,memory_bytes\n") == 0);
    CHECK(out.find("0,0.01,10,100,1000\n") != std::string::npos);
    CHECK(out.find("1,0.03,20,50,2000\n") != std::string::npos);
    CHECK(countLines(out) == 3); // header + 2 rows
}

static void testJsonExport() {
    Benchmark b;
    b.start();
    b.record(0.010, 1000);
    b.stop();

    std::ostringstream os;
    b.exportTo(os, sim::ExportFormat::Json);
    const std::string out = os.str();

    CHECK(out.front() == '[');
    CHECK(out.find("\"frame\":0") != std::string::npos);
    CHECK(out.find("\"memory_bytes\":1000") != std::string::npos);
    CHECK(out.find(']') != std::string::npos);
}

static void testExportToFileRoundTrip() {
    const std::string path = "ikore_benchmark_test.csv";
    Benchmark b;
    b.start();
    b.record(0.010, 1000);
    b.record(0.020, 2000);
    b.record(0.030, 3000);
    b.stop();

    CHECK(b.exportToFile(path, sim::ExportFormat::Csv));

    std::ifstream in(path);
    CHECK(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();
    in.close();

    CHECK(content.find("frame,time_s,frame_ms,fps,memory_bytes") == 0);
    CHECK(countLines(content) == 4); // header + 3 rows

    std::remove(path.c_str());
}

int main() {
    testCaptureLifecycle();
    testSamplesAndSummary();
    testEmptySummarySafe();
    testCsvExport();
    testJsonExport();
    testExportToFileRoundTrip();

    if (g_failures == 0) {
        std::printf("All benchmark tests passed.\n");
        return 0;
    }
    std::printf("%d benchmark test(s) failed.\n", g_failures);
    return 1;
}
