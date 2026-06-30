// Test for desync detection, diagnostics, and input-log replay - M14, #161.
//
// Verifies the issue's acceptance criteria:
//   - Desyncs are detected with an actionable diagnostic (tick + diverging state):
//     firstDivergence reports the first mismatching checksum tick with both
//     hashes, and diagnoseStates names what diverged.
//   - A recorded input log reproduces a session deterministically: replaying a
//     log (including after a serialize/parse round-trip) reproduces the exact
//     per-tick checksum trace of the original run.
//
// Built on the deterministic core (#159). Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_desync.cpp -o test_desync

#include "core/sim/Fixed.h"
#include "core/sim/Simulation.h" // DeterministicRng
#include "core/sim/StateHash.h"
#include "net/Desync.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
using namespace IKore::net;
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

struct Input {
    int move{0};
};

struct World {
    Fixed x[2];
};

static void stepWorld(World& w, const std::vector<Input>& in, int /*tick*/) {
    const Fixed speed = Fixed::fromFraction(1, 4);
    const Fixed pull = Fixed::fromFraction(1, 16);
    Fixed nx0 = w.x[0] + Fixed::fromInt(in[0].move) * speed;
    Fixed nx1 = w.x[1] + Fixed::fromInt(in[1].move) * speed;
    nx0 = nx0 + (w.x[1] - w.x[0]) * pull;
    nx1 = nx1 + (w.x[0] - w.x[1]) * pull;
    w.x[0] = nx0;
    w.x[1] = nx1;
}

static std::uint64_t hashWorld(const World& w) {
    StateHash h;
    h.addFixed(w.x[0]);
    h.addFixed(w.x[1]);
    return h.digest();
}

// Run a session: fill an input log, a checksum trace, and per-tick states.
static void runSession(const std::vector<Input>& p0, const std::vector<Input>& p1, int n,
                       InputLog<Input>& log, ChecksumTrace& trace, std::vector<World>& states) {
    log.numPlayers = 2;
    World w;
    for (int t = 0; t < n; ++t) {
        const std::vector<Input> tickInputs = {p0[static_cast<std::size_t>(t)],
                                               p1[static_cast<std::size_t>(t)]};
        log.record(tickInputs);
        stepWorld(w, tickInputs, t);
        trace.record(hashWorld(w));
        states.push_back(w);
    }
}

static std::vector<Input> scriptInputs(std::uint64_t seed, int n) {
    DeterministicRng rng(seed);
    std::vector<Input> v(static_cast<std::size_t>(n));
    for (int t = 0; t < n; ++t) v[static_cast<std::size_t>(t)].move = rng.nextInt(-1, 1);
    return v;
}

static void testReplayReproducesSession() {
    const int n = 40;
    const std::vector<Input> p0 = scriptInputs(11, n);
    const std::vector<Input> p1 = scriptInputs(22, n);

    InputLog<Input> log;
    ChecksumTrace live;
    std::vector<World> states;
    runSession(p0, p1, n, log, live, states);

    // Replaying the log reproduces the exact checksum trace.
    const ChecksumTrace replayed = replay(log, World{}, stepWorld, hashWorld);
    CHECK(!firstDivergence(live, replayed).desynced);
    CHECK(live.size() == replayed.size());

    // Replay is itself deterministic.
    const ChecksumTrace replayed2 = replay(log, World{}, stepWorld, hashWorld);
    CHECK(!firstDivergence(replayed, replayed2).desynced);

    // The log survives serialize -> parse and still reproduces the session.
    const std::string text =
        serializeLog(log, [](const Input& i) { return std::to_string(i.move); });
    const InputLog<Input> parsed =
        parseLog<Input>(text, [](const std::string& s) { return Input{std::stoi(s)}; });
    CHECK(parsed.size() == log.size());
    CHECK(parsed.numPlayers == 2);
    const ChecksumTrace fromText = replay(parsed, World{}, stepWorld, hashWorld);
    CHECK(!firstDivergence(live, fromText).desynced);
}

static void testDesyncDetectedWithDiagnostic() {
    const int n = 30;
    const std::vector<Input> p0 = scriptInputs(101, n);
    const std::vector<Input> p1 = scriptInputs(202, n);

    InputLog<Input> log;
    ChecksumTrace traceA;
    std::vector<World> statesA;
    runSession(p0, p1, n, log, traceA, statesA);

    // A second peer that diverges: flip one input at a known tick.
    const int badTick = 12;
    std::vector<Input> p1bad = p1;
    p1bad[static_cast<std::size_t>(badTick)].move += 1;
    InputLog<Input> logB;
    ChecksumTrace traceB;
    std::vector<World> statesB;
    runSession(p0, p1bad, n, logB, traceB, statesB);

    // Checksum-level detection: first mismatch is exactly at the bad tick.
    const DesyncReport r = firstDivergence(traceA, traceB);
    CHECK(r.desynced);
    CHECK(r.tick == badTick);
    CHECK(r.localHash != r.remoteHash);
    CHECK(r.summary().find("tick 12") != std::string::npos);

    // State-level diagnostic names the diverging tick and field.
    const DesyncReport diag = diagnoseStates(
        statesA, statesB, [](int, const World& a, const World& b) -> std::string {
            if (a.x[0].raw != b.x[0].raw || a.x[1].raw != b.x[1].raw) {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "x0 raw %d vs %d, x1 raw %d vs %d", a.x[0].raw,
                              b.x[0].raw, a.x[1].raw, b.x[1].raw);
                return std::string(buf);
            }
            return std::string();
        });
    CHECK(diag.desynced);
    CHECK(diag.tick == badTick);
    CHECK(!diag.detail.empty());

    // Identical traces report no desync.
    const DesyncReport same = firstDivergence(traceA, traceA);
    CHECK(!same.desynced);
    CHECK(same.tick == -1);
    CHECK(same.summary() == "in sync");
}

static void testLengthMismatchReported() {
    ChecksumTrace a;
    ChecksumTrace b;
    for (int i = 0; i < 10; ++i) {
        a.record(static_cast<std::uint64_t>(i));
        if (i < 6) b.record(static_cast<std::uint64_t>(i));
    }
    const DesyncReport r = firstDivergence(a, b);
    CHECK(r.desynced);
    CHECK(r.tick == 6); // diverges where the shorter trace ends
    CHECK(r.detail.find("length mismatch") != std::string::npos);
}

int main() {
    testReplayReproducesSession();
    testDesyncDetectedWithDiagnostic();
    testLengthMismatchReported();

    if (g_failures == 0) {
        std::printf("All desync detection / replay tests passed.\n");
        return 0;
    }
    std::printf("%d desync test(s) failed.\n", g_failures);
    return 1;
}
