// Test for fastest-clear leaderboards with replay anti-cheat - Milestone 17, #242.
//
// Verifies the acceptance criteria: a completed run produces a verifiable time that
// re-simulates to the same result, the per-level ranked list is correct, and
// invalid/desynced submissions are rejected. A greedy "client" plays each level to
// produce a real input trace; the leaderboard re-simulates it to verify.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_leaderboard.cpp -o test_leaderboard

#include "doodle/Doodle.h"
#include "game/Leaderboard.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
using namespace IKore::game;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// A room with a player, one coin, and an exit (no enemy, so a greedy path clears).
static std::string makeLevelJson() {
    doodle::LevelSpec s;
    s.walls.push_back(doodle::Wall{{{10, 0, 10}, {40, 0, 10}, {40, 0, 40}, {10, 0, 40}, {10, 0, 10}}});
    s.symbols.push_back(doodle::Symbol{"player", doodle::Vec3{15, 0, 15}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"coin", doodle::Vec3{25, 0, 25}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"exit", doodle::Vec3{34, 0, 34}, 0.0f});
    return doodle::saveLevel(s);
}

// A greedy client: each step move toward the nearest uncollected coin, else the
// exit. Records the input trace that a real player's run would produce.
static RunTrace playGreedy(const std::string& json, double dt, int maxSteps) {
    RunTrace tr;
    tr.dt = dt;
    LevelSpec spec;
    doodle::loadLevel(json, spec);
    DungeonGame g = loadGame(convert(spec));
    for (int step = 0; step < maxSteps && g.status == GameStatus::Playing; ++step) {
        ecs::Vec3 target = g.exitPosition;
        double best = 1e30;
        for (const Coin& c : g.coins) {
            if (c.collected) continue;
            const double d = std::hypot(c.position.x - g.playerPosition.x, c.position.z - g.playerPosition.z);
            if (d < best) { best = d; target = c.position; }
        }
        const GameInput in{target.x - g.playerPosition.x, target.z - g.playerPosition.z};
        tr.inputs.push_back(in);
        g.update(in, static_cast<float>(dt));
    }
    return tr;
}

// Prepend n idle frames to a trace, producing a valid-but-slower run.
static RunTrace padded(const RunTrace& base, int idleFrames) {
    RunTrace tr;
    tr.dt = base.dt;
    for (int i = 0; i < idleFrames; ++i) tr.inputs.push_back(GameInput{0.0f, 0.0f});
    for (const GameInput& in : base.inputs) tr.inputs.push_back(in);
    return tr;
}

static void testReplayIsDeterministicAndVerifiable() {
    const std::string json = makeLevelJson();
    const RunTrace tr = playGreedy(json, 1.0 / 60.0, 5000);

    const RunResult a = replayRun(json, tr);
    const RunResult b = replayRun(json, tr);
    CHECK(a.won);
    CHECK(a.steps > 0);
    CHECK(std::fabs(a.clearTime - a.steps * tr.dt) < 1e-9);
    // Same trace -> same result (the verifiable-time property).
    CHECK(a.won == b.won && a.steps == b.steps && a.clearTime == b.clearTime);

    // A trace that never reaches the exit does not clear.
    RunTrace stuck;
    stuck.dt = tr.dt;
    for (int i = 0; i < 10; ++i) stuck.inputs.push_back(GameInput{-1.0f, -1.0f}); // into a corner
    CHECK(!replayRun(json, stuck).won);

    // Unparseable level never clears.
    CHECK(!replayRun("not json", tr).won);
}

static void testRegisterUsesContentCode() {
    Leaderboard lb;
    const std::string json = makeLevelJson();
    const std::string code = lb.registerLevel(json);
    CHECK(code == shareCodeFor(json)); // keyed by the #241 share code
    CHECK(lb.hasLevel(code));
    CHECK(!lb.hasLevel("DD-0000000000000"));
}

static void testSubmitRankAndVerifiableTime() {
    Leaderboard lb;
    const std::string json = makeLevelJson();
    const std::string code = lb.registerLevel(json);

    const RunTrace fast = playGreedy(json, lb.fixedDt(), 5000);
    const RunTrace slow = padded(fast, 40);
    const double fastTime = replayRun(json, fast).clearTime;
    const double slowTime = replayRun(json, slow).clearTime;
    CHECK(slowTime > fastTime);

    // Submit with a correct claimed time (verified against the replay).
    const SubmitResult a = lb.submit(code, "alice", fast, 100, fastTime);
    CHECK(a.accepted && a.improved);
    CHECK(std::fabs(a.clearTime - fastTime) < 1e-9); // recorded time == re-simulated time
    CHECK(a.rank == 1);

    const SubmitResult b = lb.submit(code, "bob", slow, 200);
    CHECK(b.accepted);
    CHECK(b.rank == 2);

    // Ranked list: fastest first, one row per player.
    const std::vector<ScoreEntry> board = lb.top(code);
    CHECK(board.size() == 2);
    CHECK(board[0].player == "alice" && board[1].player == "bob");
    CHECK(board[0].clearTime < board[1].clearTime);
    // The recorded top time equals what the trace re-simulates to.
    CHECK(std::fabs(board[0].clearTime - fastTime) < 1e-9);
    CHECK(lb.count(code) == 2);
    CHECK(lb.rankOf(code, "bob") == 2);
    CHECK(lb.rankOf(code, "nobody") == 0);
}

static void testBestPerPlayer() {
    Leaderboard lb;
    const std::string json = makeLevelJson();
    const std::string code = lb.registerLevel(json);
    const RunTrace fast = playGreedy(json, lb.fixedDt(), 5000);
    const RunTrace slow = padded(fast, 40);

    // A player's slower run first, then a faster personal best.
    const SubmitResult first = lb.submit(code, "cara", slow, 100);
    CHECK(first.accepted && first.improved);
    const double slowTime = first.clearTime;

    const SubmitResult better = lb.submit(code, "cara", fast, 200);
    CHECK(better.accepted && better.improved);       // new personal best
    CHECK(better.clearTime < slowTime);
    CHECK(lb.count(code) == 1);                       // still one row for the player
    CHECK(std::fabs(lb.top(code)[0].clearTime - better.clearTime) < 1e-9);

    // A subsequent slower run is accepted but does not replace the best.
    const SubmitResult worse = lb.submit(code, "cara", slow, 300);
    CHECK(worse.accepted && !worse.improved);
    CHECK(std::fabs(lb.top(code)[0].clearTime - better.clearTime) < 1e-9);
    CHECK(lb.count(code) == 1);
}

static void testAntiCheatRejections() {
    Leaderboard lb;
    const std::string json = makeLevelJson();
    const std::string code = lb.registerLevel(json);
    const RunTrace fast = playGreedy(json, lb.fixedDt(), 5000);
    const double fastTime = replayRun(json, fast).clearTime;

    // Unknown level.
    const SubmitResult unknown = lb.submit("DD-0000000000000", "eve", fast, 10);
    CHECK(!unknown.accepted && unknown.reason == "unknown level");

    // Wrong timestep (a time-scaling cheat).
    RunTrace badDt = fast;
    badDt.dt = 1.0 / 30.0;
    const SubmitResult dt = lb.submit(code, "eve", badDt, 10);
    CHECK(!dt.accepted && dt.reason == "bad timestep");

    // A trace that does not clear the level.
    RunTrace noClear;
    noClear.dt = lb.fixedDt();
    for (int i = 0; i < 5; ++i) noClear.inputs.push_back(GameInput{-1.0f, -1.0f});
    const SubmitResult stuck = lb.submit(code, "eve", noClear, 10);
    CHECK(!stuck.accepted && stuck.reason == "did not clear");

    // A claimed time that disagrees with the re-simulation (tamper / desync).
    const SubmitResult desync = lb.submit(code, "eve", fast, 10, fastTime + 1.0);
    CHECK(!desync.accepted && desync.reason == "desync");

    // None of the rejected runs made it onto the board.
    CHECK(lb.count(code) == 0);
    CHECK(lb.top(code).empty());

    // A correctly claimed time is still accepted (sanity that the check is two-sided).
    const SubmitResult ok = lb.submit(code, "eve", fast, 20, fastTime);
    CHECK(ok.accepted);
    CHECK(lb.count(code) == 1);
}

int main() {
    testReplayIsDeterministicAndVerifiable();
    testRegisterUsesContentCode();
    testSubmitRankAndVerifiableTime();
    testBestPerPlayer();
    testAntiCheatRejections();

    if (g_failures == 0) {
        std::printf("All leaderboard tests passed.\n");
        return 0;
    }
    std::printf("%d leaderboard test(s) failed.\n", g_failures);
    return 1;
}
