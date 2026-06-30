// Test for GGPO-style rollback + input prediction - Milestone 14, #160.
//
// Verifies the issue's acceptance criteria:
//   - Two clients stay in sync under simulated latency and packet loss: two
//     RollbackSessions exchange inputs over a delayed, lossy channel and, once all
//     inputs are delivered, reach byte-identical state equal to a no-network
//     reference.
//   - Mispredictions roll back and resimulate without visible corruption: a
//     session that predicted wrong inputs, on receiving the authoritative ones,
//     rolls back and lands on exactly the reference state.
//
// Built on the deterministic core (#159): integer Fixed state + StateHash.
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_rollback.cpp -o test_rollback

#include "core/sim/Fixed.h"
#include "core/sim/Simulation.h" // DeterministicRng
#include "core/sim/StateHash.h"
#include "net/Rollback.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace IKore;
using IKore::net::RollbackSession;
using IKore::sim::DeterministicRng;
using IKore::sim::Fixed;
using IKore::sim::StateHash;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// --- The shared deterministic simulation (used by clients and the reference) ---

struct Input {
    int move{0};
    bool operator==(const Input& o) const { return move == o.move; }
};

struct World {
    Fixed x[2];
};

// Each agent moves by its input, then is pulled toward the other agent - so a
// wrong predicted input for one player corrupts both positions until corrected,
// making rollback observable.
static void stepWorld(World& w, const std::vector<Input>& in, int /*frame*/) {
    const Fixed speed = Fixed::fromFraction(1, 4);
    const Fixed pull = Fixed::fromFraction(1, 16);
    Fixed nx0 = w.x[0] + Fixed::fromInt(in[0].move) * speed;
    Fixed nx1 = w.x[1] + Fixed::fromInt(in[1].move) * speed;
    nx0 = nx0 + (w.x[1] - w.x[0]) * pull;
    nx1 = nx1 + (w.x[0] - w.x[1]) * pull;
    w.x[0] = nx0;
    w.x[1] = nx1;
}

static uint64_t hashWorld(const World& w) {
    StateHash h;
    h.addFixed(w.x[0]);
    h.addFixed(w.x[1]);
    return h.digest();
}

// Ground truth: simulate N frames with all inputs authoritative (no prediction).
static World referenceRun(const std::vector<Input>& p0, const std::vector<Input>& p1, int n) {
    World w;
    for (int f = 0; f < n; ++f) {
        std::vector<Input> in = {p0[static_cast<std::size_t>(f)], p1[static_cast<std::size_t>(f)]};
        stepWorld(w, in, f);
    }
    return w;
}

static std::vector<Input> scriptInputs(std::uint64_t seed, int n) {
    DeterministicRng rng(seed);
    std::vector<Input> v(static_cast<std::size_t>(n));
    for (int f = 0; f < n; ++f) v[static_cast<std::size_t>(f)].move = rng.nextInt(-1, 1);
    return v;
}

static void testNoRollbackWhenPredictionIsCorrect() {
    // Player 1 always inputs 0, which equals the default prediction.
    RollbackSession<World, Input> s(2, /*local=*/0, World{}, stepWorld);
    const int n = 10;
    for (int f = 0; f < n; ++f) s.advanceFrame(Input{1});
    for (int f = 0; f < n; ++f) s.addRemoteInput(1, f, Input{0}); // matches prediction

    CHECK(s.rollbackCount() == 0); // nothing was mispredicted
    std::vector<Input> p0(static_cast<std::size_t>(n), Input{1});
    std::vector<Input> p1(static_cast<std::size_t>(n), Input{0});
    CHECK(hashWorld(s.state()) == hashWorld(referenceRun(p0, p1, n)));
}

static void testMispredictionRollsBackToCorrectState() {
    RollbackSession<World, Input> s(2, /*local=*/0, World{}, stepWorld);
    const int n = 12;
    std::vector<Input> p0 = scriptInputs(1, n);
    std::vector<Input> p1 = scriptInputs(2, n); // varied -> default 0 prediction is wrong

    for (int f = 0; f < n; ++f) s.advanceFrame(p0[static_cast<std::size_t>(f)]);
    // The authoritative remote inputs arrive late; delivering them corrects state.
    for (int f = 0; f < n; ++f) s.addRemoteInput(1, f, p1[static_cast<std::size_t>(f)]);

    CHECK(s.rollbackCount() > 0); // at least one misprediction was corrected
    CHECK(s.confirmedFrame() == n);
    CHECK(hashWorld(s.state()) == hashWorld(referenceRun(p0, p1, n))); // no corruption
}

// One scheduled remote-input delivery.
struct Delivery {
    int arrival; // frame at which it is received
    int frame;   // frame the input is for
    Input input;
};

static void testTwoClientsStaySyncedUnderLatencyAndLoss() {
    const int n = 60;
    const int latency = 4;
    const std::vector<Input> p0 = scriptInputs(101, n);
    const std::vector<Input> p1 = scriptInputs(202, n);

    RollbackSession<World, Input> a(2, /*local=*/0, World{}, stepWorld, /*window=*/1024);
    RollbackSession<World, Input> b(2, /*local=*/1, World{}, stepWorld, /*window=*/1024);

    std::vector<Delivery> toA; // player 1 inputs headed to client A
    std::vector<Delivery> toB; // player 0 inputs headed to client B

    for (int f = 0; f < n; ++f) {
        a.advanceFrame(p0[static_cast<std::size_t>(f)]);
        b.advanceFrame(p1[static_cast<std::size_t>(f)]);

        // Schedule cross-delivery with latency; "lost" packets arrive much later.
        const bool lostA = (f % 7) == 3;
        const bool lostB = (f % 5) == 2;
        toA.push_back({f + latency + (lostA ? 20 : 0), f, p1[static_cast<std::size_t>(f)]});
        toB.push_back({f + latency + (lostB ? 20 : 0), f, p0[static_cast<std::size_t>(f)]});

        for (const Delivery& d : toA) {
            if (d.arrival == f) a.addRemoteInput(1, d.frame, d.input);
        }
        for (const Delivery& d : toB) {
            if (d.arrival == f) b.addRemoteInput(0, d.frame, d.input);
        }
    }

    // Flush every remaining (delayed/lost) input in increasing frame order.
    auto byFrame = [](const Delivery& x, const Delivery& y) { return x.frame < y.frame; };
    std::sort(toA.begin(), toA.end(), byFrame);
    std::sort(toB.begin(), toB.end(), byFrame);
    for (const Delivery& d : toA) {
        if (d.arrival >= n) a.addRemoteInput(1, d.frame, d.input);
    }
    for (const Delivery& d : toB) {
        if (d.arrival >= n) b.addRemoteInput(0, d.frame, d.input);
    }

    const World ref = referenceRun(p0, p1, n);
    // Both clients converged to each other and to the authoritative reference.
    CHECK(hashWorld(a.state()) == hashWorld(b.state()));
    CHECK(hashWorld(a.state()) == hashWorld(ref));
    CHECK(hashWorld(b.state()) == hashWorld(ref));
    // Rollback was actually exercised on both sides, and everything is confirmed.
    CHECK(a.rollbackCount() > 0);
    CHECK(b.rollbackCount() > 0);
    CHECK(a.confirmedFrame() == n);
    CHECK(b.confirmedFrame() == n);
}

int main() {
    testNoRollbackWhenPredictionIsCorrect();
    testMispredictionRollsBackToCorrectState();
    testTwoClientsStaySyncedUnderLatencyAndLoss();

    if (g_failures == 0) {
        std::printf("All rollback netcode tests passed.\n");
        return 0;
    }
    std::printf("%d rollback netcode test(s) failed.\n", g_failures);
    return 1;
}
