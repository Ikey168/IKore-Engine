// Test for UGC share codes / feed / stars - Milestone 17, #174.
//
// Verifies the issue's acceptance criteria: a user can publish a map and others
// can find (by share code) and play it, and stars plus a browsable feed
// (New / Top / Hot) work end to end. The level payload is the doodle-level JSON,
// so a fetched entry loads straight back into a playable game.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_level_catalog.cpp -o test_level_catalog

#include "doodle/Doodle.h"
#include "game/LevelCatalog.h"

#include <cstdio>
#include <string>

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

static std::string makeLevelJson() {
    doodle::LevelSpec s;
    s.walls.push_back(doodle::Wall{{{10, 0, 10}, {40, 0, 10}, {40, 0, 40}, {10, 0, 40}, {10, 0, 10}}});
    s.symbols.push_back(doodle::Symbol{"player", doodle::Vec3{15, 0, 15}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"exit", doodle::Vec3{35, 0, 35}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"coin", doodle::Vec3{25, 0, 25}, 0.0f});
    return doodle::saveLevel(s);
}

static void testPublishFindAndPlay() {
    LevelCatalog catalog;
    const std::string codeA = catalog.publish("alice", "Castle", makeLevelJson(), 100);
    const std::string codeB = catalog.publish("bob", "Cave", makeLevelJson(), 200);

    CHECK(catalog.size() == 2);
    CHECK(codeA != codeB);                 // unique share codes
    CHECK(catalog.getByCode("DD-zzzzzz") == nullptr); // unknown code

    const LevelEntry* e = catalog.getByCode(codeA);
    CHECK(e != nullptr);
    CHECK(e->author == "alice" && e->title == "Castle");

    // Another user fetches by code and plays the exact map.
    doodle::LevelSpec loaded;
    CHECK(doodle::loadLevel(e->levelJson, loaded));
    const doodle::DungeonGame game = doodle::loadGame(doodle::buildScene(loaded));
    CHECK(game.hasExit);
    CHECK(game.totalCoins == 1);
    CHECK(game.playerPosition.x == 15.0f && game.playerPosition.z == 15.0f);
}

static void testStarsAndTopFeed() {
    LevelCatalog catalog;
    const std::string codeA = catalog.publish("alice", "Castle", makeLevelJson(), 100);
    const std::string codeB = catalog.publish("bob", "Cave", makeLevelJson(), 200);

    CHECK(catalog.star(codeA));
    CHECK(catalog.star(codeA));
    CHECK(catalog.star(codeA));
    CHECK(catalog.star(codeB));
    CHECK(!catalog.star("DD-nope")); // unknown code

    CHECK(catalog.getByCode(codeA)->stars == 3);

    const std::vector<LevelEntry> top = catalog.browseTop();
    CHECK(top.size() == 2);
    CHECK(top[0].code == codeA); // most stars first
    CHECK(top[1].code == codeB);
}

static void testNewAndHotFeeds() {
    LevelCatalog catalog;
    const std::string codeA = catalog.publish("alice", "Castle", makeLevelJson(), 100);
    const std::string codeB = catalog.publish("bob", "Cave", makeLevelJson(), 200);

    // New: most recently published first.
    const std::vector<LevelEntry> fresh = catalog.browseNew();
    CHECK(fresh[0].code == codeB);
    CHECK(fresh[1].code == codeA);

    // Hot with light stars: the newer map wins.
    catalog.star(codeA);
    catalog.star(codeB);
    CHECK(catalog.browseHot(250)[0].code == codeB);

    // Hot rewards popularity: many stars lift the older map above the newer one.
    for (int i = 0; i < 50; ++i) catalog.star(codeA);
    CHECK(catalog.browseHot(250)[0].code == codeA);
}

static void testRecordPlayAndWeeklyPrompt() {
    LevelCatalog catalog;
    const std::string code = catalog.publish("alice", "Castle", makeLevelJson(), 100);
    CHECK(catalog.recordPlay(code));
    CHECK(!catalog.recordPlay("DD-nope"));
    CHECK(catalog.getByCode(code)->plays == 1);

    // Weekly prompts rotate and wrap.
    CHECK(!LevelCatalog::weeklyPrompt(0).empty());
    CHECK(LevelCatalog::weeklyPrompt(0) != LevelCatalog::weeklyPrompt(1));
    CHECK(LevelCatalog::weeklyPrompt(0) == LevelCatalog::weeklyPrompt(6)); // 6 prompts, wraps
}

int main() {
    testPublishFindAndPlay();
    testStarsAndTopFeed();
    testNewAndHotFeeds();
    testRecordPlayAndWeeklyPrompt();

    if (g_failures == 0) {
        std::printf("All level catalog (UGC) tests passed.\n");
        return 0;
    }
    std::printf("%d level catalog test(s) failed.\n", g_failures);
    return 1;
}
