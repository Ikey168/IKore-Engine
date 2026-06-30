// Test for the deterministic fixed-step simulation loop - Milestone 12, #152.
//
// Verifies the issue's acceptance criteria:
//   - Same seed + same inputs yields identical state across runs (and a different
//     seed yields different state).
//   - Rendering is decoupled from the fixed step: the number of fixed steps and
//     the resulting state depend only on elapsed time, not how it is chunked per
//     frame; a leftover alpha is exposed for interpolation.
// Pure std, header-only:
//   g++ -std=c++17 -I src tests/test_simulation.cpp -o test_simulation

#include "core/sim/Simulation.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::sim;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

// Run a small world (5 agents) for the given per-frame deltas; each fixed step
// nudges every agent by a deterministic random amount. Returns final positions.
static std::vector<float> runWorld(std::uint64_t seed, double fixedDelta, int maxSteps,
                                   const std::vector<double>& frameDeltas) {
    Simulation sim(seed, fixedDelta, maxSteps);
    std::vector<float> world(5, 0.0f);
    for (double dt : frameDeltas) {
        sim.advance(dt, [&world](std::uint64_t, DeterministicRng& rng) {
            for (float& v : world) v += rng.nextFloat();
        });
    }
    return world;
}

static void testRngDeterminism() {
    DeterministicRng a(123), b(123), c(124);
    bool sameAB = true, diffAC = false;
    for (int i = 0; i < 16; ++i) {
        const std::uint64_t va = a.nextU64();
        if (va != b.nextU64()) sameAB = false;
        if (va != c.nextU64()) diffAC = true;
    }
    CHECK(sameAB);  // same seed -> identical sequence
    CHECK(diffAC);  // different seed -> diverges
}

static void testFixedStepDecoupling() {
    FixedTimestep ts;
    ts.fixedDelta = 1.0; // whole seconds: exact, no FP boundary fuzz
    ts.maxSteps = 1000;

    CHECK(ts.stepsFor(0.0) == 0);
    CHECK(approx(ts.alpha(), 0.0));

    CHECK(ts.stepsFor(2.5) == 2);      // 2 whole steps...
    CHECK(approx(ts.alpha(), 0.5));    // ...with 0.5 left over for interpolation

    CHECK(ts.stepsFor(0.6) == 1);      // 0.5 + 0.6 = 1.1 -> 1 step
    CHECK(approx(ts.alpha(), 0.1));

    // Spiral-of-death guard: a huge stall is capped.
    FixedTimestep capped;
    capped.fixedDelta = 1.0;
    capped.maxSteps = 8;
    CHECK(capped.stepsFor(100.0) == 8);
}

static void testReproducibleAcrossRunsAndChunking() {
    // Same seed + same inputs -> identical state.
    const std::vector<double> deltas(20, 1.0);
    const std::vector<float> run1 = runWorld(7, 1.0, 1000, deltas);
    const std::vector<float> run2 = runWorld(7, 1.0, 1000, deltas);
    CHECK(run1 == run2);

    // Different seed -> different state.
    const std::vector<float> run3 = runWorld(8, 1.0, 1000, deltas);
    CHECK(run1 != run3);

    // Decoupled from frame chunking: 20s as one frame vs twenty 1s frames vs
    // forty 0.5s frames all produce the same state (same total fixed steps).
    const std::vector<float> oneBigFrame = runWorld(7, 1.0, 1000, {20.0});
    std::vector<double> manySmall(40, 0.5);
    const std::vector<float> manyFrames = runWorld(7, 1.0, 1000, manySmall);
    CHECK(run1 == oneBigFrame);
    CHECK(run1 == manyFrames);
}

static void testSimulationTickCount() {
    Simulation sim(1, 1.0, 1000);
    int ran = sim.advance(10.0, [](std::uint64_t, DeterministicRng&) {});
    CHECK(ran == 10);
    CHECK(sim.tick() == 10);
    ran = sim.advance(0.4, [](std::uint64_t, DeterministicRng&) {});
    CHECK(ran == 0);                 // not a full step yet
    CHECK(approx(sim.alpha(), 0.4)); // leftover available to the renderer
}

int main() {
    testRngDeterminism();
    testFixedStepDecoupling();
    testReproducibleAcrossRunsAndChunking();
    testSimulationTickCount();

    if (g_failures == 0) {
        std::printf("All simulation tests passed.\n");
        return 0;
    }
    std::printf("%d simulation test(s) failed.\n", g_failures);
    return 1;
}
