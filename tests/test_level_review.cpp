// Test for the confidence + repair review model - Milestone 16, #170.
//
// Verifies the issue's acceptance criteria: users can review and correct the
// interpretation before playing, and the system never fails silently on ambiguous
// input (every problem is surfaced and gates play until resolved). Ends by handing
// the reviewed level to the M15 converter + game for a playable scene.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_level_review.cpp -o test_level_review

#include "cv/Symbols.h"
#include "cv/Vectorize.h"
#include "game/DoodleScene.h"
#include "game/DungeonGame.h"
#include "game/LevelReview.h"

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

static cv::SymbolInstance sym(float x, float y, const std::string& obj, float conf, bool low) {
    cv::SymbolInstance s;
    s.cx = x;
    s.cy = y;
    s.result.object = obj;
    s.result.confidence = conf;
    s.result.lowConfidence = low;
    return s;
}

static std::vector<cv::Polyline> roomWalls() {
    return {{{10, 10}, {50, 10}, {50, 50}, {10, 50}, {10, 10}}};
}

static bool hasIssue(const std::vector<LevelIssue>& issues, IssueKind kind) {
    for (const LevelIssue& i : issues) {
        if (i.kind == kind) return true;
    }
    return false;
}

static void testAssembleFlagsLowConfidence() {
    const std::vector<cv::SymbolInstance> symbols = {
        sym(15, 15, "player", 0.9f, false),
        sym(45, 45, "exit", 0.85f, false),
        sym(30, 30, "coin", 0.8f, false),
        sym(20, 40, "coin", 0.2f, true), // ambiguous, defaulted -> needs review
    };
    InterpretedLevel level = assembleLevel(roomWalls(), symbols, 1.0f);

    CHECK(level.symbols.size() == 4);
    CHECK(level.walls.size() == 1);
    CHECK(level.pendingReviewCount() == 1);
    CHECK(level.symbols[0].position.x == 15.0f && level.symbols[0].position.z == 15.0f);

    // The ambiguous mark is surfaced, not dropped, and blocks play.
    const std::vector<LevelIssue> issues = level.issues();
    CHECK(hasIssue(issues, IssueKind::LowConfidenceSymbol));
    CHECK(!level.readyToPlay());

    // User corrects the misread; it is now reviewed and no longer blocks.
    level.setSymbolType(3, "enemy");
    CHECK(level.pendingReviewCount() == 0);
    CHECK(level.readyToPlay());
    CHECK(level.symbols[3].type == "enemy");
}

static void testMissingStartAndExitAreReported() {
    const std::vector<cv::SymbolInstance> symbols = {sym(30, 30, "coin", 0.8f, false)};
    InterpretedLevel level = assembleLevel(roomWalls(), symbols, 1.0f);

    std::vector<LevelIssue> issues = level.issues();
    CHECK(hasIssue(issues, IssueKind::NoPlayerStart));
    CHECK(hasIssue(issues, IssueKind::NoExit));
    CHECK(!level.readyToPlay());

    level.addSymbol("player", ecs::Vec3{15, 0, 15});
    level.addSymbol("exit", ecs::Vec3{45, 0, 45});
    CHECK(level.readyToPlay());
}

static void testEditsAndSilentFailureGuard() {
    InterpretedLevel level = assembleLevel(
        roomWalls(),
        {sym(15, 15, "player", 0.9f, false), sym(45, 45, "exit", 0.9f, false),
         sym(30, 30, "coin", 0.8f, false)},
        1.0f);
    CHECK(level.readyToPlay());

    // Move / add / delete edits.
    level.moveSymbol(2, ecs::Vec3{33, 0, 33});
    CHECK(level.symbols[2].position.x == 33.0f);
    level.addWall(Wall{{{60, 10, 0}, {60, 0, 50}}});
    CHECK(level.walls.size() == 2);
    level.deleteWall(1);
    CHECK(level.walls.size() == 1);

    // Deleting the player start is caught (never fails silently).
    for (std::size_t i = 0; i < level.symbols.size(); ++i) {
        if (level.symbols[i].type == "player") {
            level.deleteSymbol(i);
            break;
        }
    }
    CHECK(!level.readyToPlay());
    CHECK(hasIssue(level.issues(), IssueKind::NoPlayerStart));
}

static void testReviewedLevelIsPlayable() {
    InterpretedLevel level = assembleLevel(
        roomWalls(),
        {sym(15, 15, "player", 0.9f, false), sym(45, 45, "exit", 0.85f, false),
         sym(30, 30, "coin", 0.8f, false), sym(20, 40, "coin", 0.2f, true)},
        1.0f);
    level.setSymbolType(3, "enemy"); // resolve the ambiguous mark
    CHECK(level.readyToPlay());

    // Hand the reviewed level to the converter + game (the M15 pipeline).
    const LevelSpec spec = level.toLevelSpec();
    const SceneDescription scene = convert(spec);
    CHECK(!scene.wallBoxes.empty());

    const DungeonGame game = loadGame(scene);
    CHECK(game.hasExit);
    CHECK(game.totalCoins == 1);        // the one coin
    CHECK(game.enemies.size() == 1);    // the corrected enemy
    CHECK(game.playerPosition.x == 15.0f && game.playerPosition.z == 15.0f);
}

int main() {
    testAssembleFlagsLowConfidence();
    testMissingStartAndExitAreReported();
    testEditsAndSilentFailureGuard();
    testReviewedLevelIsPlayable();

    if (g_failures == 0) {
        std::printf("All level review tests passed.\n");
        return 0;
    }
    std::printf("%d level review test(s) failed.\n", g_failures);
    return 1;
}
