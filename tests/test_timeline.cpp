// Test for time control + rewind/replay - Milestone 12, #154.
//
// Verifies the issue's acceptance criteria:
//   - The simulation can be paused, single-stepped, and sped up / slowed down.
//   - Rewinding to an earlier tick and replaying forward reproduces the same
//     state (exercising RNG-state snapshot/restore, so replay is bit-identical).
// Plus the bounded history window (ring buffer eviction) behaviour.
//
// Pure std + header-only sim primitives:
//   g++ -std=c++17 -I src tests/test_timeline.cpp -o test_timeline

#include "core/sim/Timeline.h"

#include <cmath>
#include <cstdint>
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

// Step that just counts ticks (ignores the RNG) - for the control tests.
static void countStep(int& s, std::uint64_t, DeterministicRng&) { ++s; }

static void testPauseAndSingleStep() {
    Timeline<int> tl(0, /*seed=*/1, /*fixedDelta=*/1.0, /*historyCapacity=*/1000,
                     /*maxStepsPerFrame=*/1000);

    // Running: 2.5s at fixedDelta 1.0 -> 2 steps, 0.5 left over.
    CHECK(tl.advance(2.5, countStep) == 2);
    CHECK(tl.tick() == 2);
    CHECK(tl.state() == 2);
    CHECK(approx(tl.alpha(), 0.5));

    // Paused: real time no longer advances the sim.
    tl.pause();
    CHECK(tl.paused());
    CHECK(tl.advance(100.0, countStep) == 0);
    CHECK(tl.tick() == 2);

    // Single-step works while paused, even with a zero frame delta.
    tl.requestStep(1);
    CHECK(tl.advance(0.0, countStep) == 1);
    CHECK(tl.tick() == 3);
    CHECK(tl.state() == 3);

    // Multiple queued steps run together.
    tl.requestStep(2);
    CHECK(tl.advance(0.0, countStep) == 2);
    CHECK(tl.tick() == 5);
    CHECK(tl.state() == 5);
}

static void testVariableSpeed() {
    // Double speed: 1s of real time yields 2 steps at fixedDelta 1.0.
    Timeline<int> fast(0, 1, 1.0, 1000, 1000);
    fast.setTimeScale(2.0);
    CHECK(approx(fast.timeScale(), 2.0));
    CHECK(fast.advance(1.0, countStep) == 2);
    CHECK(fast.tick() == 2);

    // Half speed: 1s yields 0 steps (0.5 accumulated), the next 1s completes one.
    Timeline<int> slow(0, 1, 1.0, 1000, 1000);
    slow.setTimeScale(0.5);
    CHECK(slow.advance(1.0, countStep) == 0);
    CHECK(approx(slow.alpha(), 0.5));
    CHECK(slow.advance(1.0, countStep) == 1);
    CHECK(slow.tick() == 1);

    // Negative scale is clamped to a freeze.
    Timeline<int> frozen(0, 1, 1.0, 1000, 1000);
    frozen.setTimeScale(-3.0);
    CHECK(approx(frozen.timeScale(), 0.0));
    CHECK(frozen.advance(10.0, countStep) == 0);
}

// A small world perturbed by the RNG each step, so reproducing it across a
// rewind/replay proves the RNG stream is snapshotted and restored correctly.
struct World {
    double a{0.0};
    double b{0.0};
    bool operator==(const World& o) const { return a == o.a && b == o.b; }
};

static void perturb(World& w, std::uint64_t, DeterministicRng& rng) {
    w.a += rng.nextDouble();
    w.b += rng.nextDouble() * 2.0 - 1.0;
}

static void testRewindReplayReproducesState() {
    Timeline<World> tl(World{}, /*seed=*/12345, /*fixedDelta=*/1.0, /*historyCapacity=*/64,
                       /*maxStepsPerFrame=*/1000);

    // Record the full trajectory: state at tick 0, 1, ... 50.
    std::vector<World> traj;
    traj.push_back(tl.state());
    for (int t = 1; t <= 50; ++t) {
        tl.advance(1.0, perturb);
        traj.push_back(tl.state());
    }
    CHECK(tl.tick() == 50);

    // Rewind to tick 20: state is exactly what it was then.
    CHECK(tl.rewindTo(20));
    CHECK(tl.tick() == 20);
    CHECK(tl.state() == traj[20]);

    // Replay forward: every tick reproduces the original trajectory.
    bool replayMatches = true;
    for (int t = 21; t <= 50; ++t) {
        tl.advance(1.0, perturb);
        if (!(tl.state() == traj[static_cast<std::size_t>(t)])) replayMatches = false;
    }
    CHECK(replayMatches);
    CHECK(tl.tick() == 50);

    // Rewinding to the current tick is a no-op success; the future is rejected.
    CHECK(tl.rewindTo(50));
    CHECK(!tl.rewindTo(51));
}

static void testHistoryWindow() {
    // Capacity 16: after 50 steps only the most recent 16 ticks are recoverable.
    Timeline<int> tl(0, 1, 1.0, /*historyCapacity=*/16, 1000);
    for (int i = 0; i < 50; ++i) tl.advance(1.0, countStep);

    CHECK(tl.tick() == 50);
    CHECK(tl.historySize() == 16);
    CHECK(tl.oldestTick() == 35); // ticks 35..50 retained
    CHECK(tl.canRewindTo(35));
    CHECK(!tl.canRewindTo(34)); // evicted
    CHECK(tl.canRewindTo(50));
    CHECK(!tl.canRewindTo(51)); // future

    CHECK(!tl.rewindTo(34));     // outside the window: no change
    CHECK(tl.tick() == 50);

    CHECK(tl.rewindTo(40));      // inside the window
    CHECK(tl.tick() == 40);
    CHECK(tl.state() == 40);
}

int main() {
    testPauseAndSingleStep();
    testVariableSpeed();
    testRewindReplayReproducesState();
    testHistoryWindow();

    if (g_failures == 0) {
        std::printf("All timeline (time control + rewind) tests passed.\n");
        return 0;
    }
    std::printf("%d timeline test(s) failed.\n", g_failures);
    return 1;
}
