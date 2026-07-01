// Test for the performance-overlay stats core - Milestone 9, #53.
//
// Verifies the headless aggregation behind the ImGui FPS/perf overlay: FPS and
// frame time (instantaneous and averaged), min/max, the bounded rolling history
// for the graph, and the resident-memory readout. The ImGui panel that draws
// these lives in DebugUI and is exercised in the engine build.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_perf_stats.cpp -o test_perf_stats

#include "core/PerfStats.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(double a, double b, double eps = 1e-2) { return std::fabs(a - b) <= eps; }

static void testSteadyFrames() {
    PerfStats p;
    for (int i = 0; i < 100; ++i) p.record(1.0 / 60.0); // steady 60 FPS

    CHECK(approx(p.lastFrameMs(), 1000.0 / 60.0));
    CHECK(approx(p.fps(), 60.0, 0.1));
    CHECK(approx(p.avgFrameMs(), 1000.0 / 60.0));
    CHECK(approx(p.avgFps(), 60.0, 0.1));
    CHECK(approx(p.minFrameMs(), p.maxFrameMs())); // all equal
    CHECK(p.frameCount() == 100);
}

static void testVariedFramesMinMaxAvg() {
    PerfStats p;
    for (int i = 0; i < 10; ++i) {
        p.record(1.0 / 60.0); // ~16.667 ms
        p.record(1.0 / 30.0); // ~33.333 ms
    }
    CHECK(approx(p.minFrameMs(), 1000.0 / 60.0));
    CHECK(approx(p.maxFrameMs(), 1000.0 / 30.0));
    CHECK(approx(p.avgFrameMs(), (1000.0 / 60.0 + 1000.0 / 30.0) / 2.0, 0.05));
}

static void testHistoryIsBounded() {
    PerfStats p(10); // small ring
    for (int i = 0; i < 25; ++i) p.record(0.01);
    CHECK(p.historySize() == 10);       // capped at capacity
    CHECK(p.frameCount() == 25);        // total count keeps growing
    const std::vector<float> hist = p.historyMs();
    CHECK(hist.size() == 10);
    CHECK(approx(hist.back(), 10.0));   // last recorded (0.01 s -> 10 ms)
}

static void testZeroFrameIsSafe() {
    PerfStats p;
    p.record(0.0);
    CHECK(p.fps() == 0.0); // no division by zero
    CHECK(p.lastFrameMs() == 0.0f);
}

static void testMemoryReadout() {
    PerfStats p;
    p.refreshMemory();
    const long bytes = p.memoryBytes();
#if defined(__linux__)
    CHECK(bytes > 0); // this test process has a resident set
#else
    CHECK(bytes >= 0);
#endif
    CHECK(PerfStats::residentBytes() >= 0);
}

int main() {
    testSteadyFrames();
    testVariedFramesMinMaxAvg();
    testHistoryIsBounded();
    testZeroFrameIsSafe();
    testMemoryReadout();

    if (g_failures == 0) {
        std::printf("All perf-stats tests passed.\n");
        return 0;
    }
    std::printf("%d perf-stats test(s) failed.\n", g_failures);
    return 1;
}
