// Test for UGC Phase 5: share codes + level browser - Milestone 17, #241.
//
// Verifies the acceptance criteria: a level publishes to a stable, content-addressed
// code and re-imports byte-identical on another instance (no shared backend), the
// browser lists catalog entries with a playable preview, and everything reuses the
// existing doodle-level JSON (no bespoke serialization).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_level_share.cpp -o test_level_share

#include "doodle/Doodle.h"
#include "game/LevelBrowser.h"
#include "game/LevelCatalog.h"
#include "game/LevelShare.h"

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

static int countChar(const std::string& s, char c) {
    int n = 0;
    for (char ch : s) n += (ch == c);
    return n;
}

static std::string makeLevelJson() {
    doodle::LevelSpec s;
    s.walls.push_back(doodle::Wall{{{10, 0, 10}, {40, 0, 10}, {40, 0, 40}, {10, 0, 40}, {10, 0, 10}}});
    s.symbols.push_back(doodle::Symbol{"player", doodle::Vec3{15, 0, 15}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"exit", doodle::Vec3{35, 0, 35}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"coin", doodle::Vec3{25, 0, 25}, 0.0f});
    s.symbols.push_back(doodle::Symbol{"enemy", doodle::Vec3{30, 0, 20}, 0.0f});
    return doodle::saveLevel(s);
}

static void testStableContentCode() {
    const std::string a = makeLevelJson();
    const std::string b = makeLevelJson();
    // Same content -> same code (stable across instances); format is DD- + 13 chars.
    CHECK(shareCodeFor(a) == shareCodeFor(b));
    CHECK(shareCodeFor(a).rfind("DD-", 0) == 0);
    CHECK(shareCodeFor(a).size() == 3 + 13);
    // Any content change -> a different code.
    CHECK(shareCodeFor(a) != shareCodeFor(a + " "));
}

static void testBase64RoundTrip() {
    // Every byte value round-trips.
    std::string bytes;
    for (int i = 0; i < 256; ++i) bytes += static_cast<char>(i);
    std::string enc = detail::base64Encode(bytes), dec;
    CHECK(detail::base64Decode(enc, dec));
    CHECK(dec == bytes);
    // Empty and short inputs.
    std::string e;
    CHECK(detail::base64Decode(detail::base64Encode(""), e) && e.empty());
    CHECK(detail::base64Decode(detail::base64Encode("A"), e) && e == "A");
    CHECK(detail::base64Decode(detail::base64Encode("AB"), e) && e == "AB");
    // A 3-char remainder is valid (decodes to 2 bytes).
    CHECK(detail::base64Decode("abc", e));
    // Invalid input rejected.
    CHECK(!detail::base64Decode("a", e));       // length % 4 == 1 is never valid
    CHECK(!detail::base64Decode("****", e));    // characters outside the alphabet
}

static void testShareRoundTripAndIntegrity() {
    const std::string json = makeLevelJson();

    // Deterministic and self-contained.
    const std::string share = encodeShare(json);
    CHECK(share == encodeShare(json));
    CHECK(share.rfind("DDL1:", 0) == 0);

    const ShareImport imp = decodeShare(share);
    CHECK(imp.ok);
    CHECK(imp.levelJson == json);           // byte-identical
    CHECK(imp.code == shareCodeFor(json));  // carries the stable content code

    // Integrity: any corruption is rejected rather than importing a wrong level.
    std::string tampered = share;
    tampered[tampered.size() - 3] = (tampered[tampered.size() - 3] == 'A') ? 'B' : 'A';
    CHECK(!decodeShare(tampered).ok);
    CHECK(!decodeShare("DDL1:DD-0000000000000:" + detail::base64Encode(json)).ok); // wrong code
    CHECK(!decodeShare("XYZ:whatever").ok); // wrong prefix
    CHECK(!decodeShare("").ok);
    CHECK(!decodeShare("DDL1:onlycode").ok); // missing payload separator
}

static void testCrossInstanceReimport() {
    // "Instance A" publishes; "instance B" imports the share string with no shared
    // state and gets a byte-identical, playable level.
    const std::string original = makeLevelJson();
    const std::string share = encodeShare(original);

    const ShareImport onB = decodeShare(share);
    CHECK(onB.ok);
    CHECK(onB.levelJson == original);

    doodle::LevelSpec loaded;
    CHECK(doodle::loadLevel(onB.levelJson, loaded));
    const doodle::DungeonGame game = doodle::loadGame(doodle::buildScene(loaded));
    CHECK(game.hasExit);
    CHECK(game.totalCoins == 1);
    CHECK(game.playerPosition.x == 15.0f && game.playerPosition.z == 15.0f);
}

static void testBrowserFeedsAndSelection() {
    LevelCatalog catalog;
    const std::string a = catalog.publish("alice", "Castle", makeLevelJson(), 100);
    const std::string b = catalog.publish("bob", "Cave", makeLevelJson(), 200);
    catalog.star(a);
    catalog.star(a);
    catalog.star(b);

    LevelBrowser browser(catalog, /*now=*/250);
    CHECK(browser.size() == 2);

    // New feed: newest first.
    browser.setFeed(Feed::New);
    CHECK(browser.entries()[0].code == b);
    CHECK(browser.entries()[1].code == a);

    // Top feed: most stars first.
    browser.setFeed(Feed::Top);
    CHECK(browser.entries()[0].code == a);

    // Selection and clamped movement.
    CHECK(browser.select(1));
    CHECK(browser.selectedIndex() == 1);
    CHECK(!browser.select(9)); // out of range leaves selection unchanged
    CHECK(browser.selectedIndex() == 1);
    browser.moveSelection(5);
    CHECK(browser.selectedIndex() == 1); // clamped to last
    browser.moveSelection(-5);
    CHECK(browser.selectedIndex() == 0); // clamped to first

    // Refresh picks up a newly published level.
    catalog.publish("cara", "Crypt", makeLevelJson(), 300);
    browser.setFeed(Feed::New);
    CHECK(browser.size() == 3);
    CHECK(browser.entries()[0].title == "Crypt");
}

static void testBrowserPreviewAndPlay() {
    LevelCatalog catalog;
    catalog.publish("alice", "Castle", makeLevelJson(), 100);
    LevelBrowser browser(catalog, 100);
    browser.select(0);

    const LevelPreview pv = browser.preview(24, 12);
    CHECK(pv.valid);
    CHECK(pv.playable()); // parsed, has a player spawn and an exit
    CHECK(pv.wallCount == 1);
    CHECK(pv.coinCount == 1);
    CHECK(pv.enemyCount == 1);
    CHECK(pv.hasPlayer && pv.hasExit);
    CHECK(pv.width == 24 && pv.height == 12);
    // ASCII map: gridH rows of gridW chars plus a newline each.
    CHECK(pv.ascii.size() == static_cast<std::size_t>(24 + 1) * 12);
    CHECK(countChar(pv.ascii, '\n') == 12);
    CHECK(countChar(pv.ascii, '#') > 0); // walls rendered
    CHECK(countChar(pv.ascii, 'P') == 1);
    CHECK(countChar(pv.ascii, 'E') == 1);
    CHECK(countChar(pv.ascii, 'o') == 1);

    // play() records the play and returns the exact level JSON.
    std::string outJson;
    CHECK(browser.play(outJson));
    CHECK(outJson == makeLevelJson());
    CHECK(catalog.getByCode(browser.selected()->code)->plays == 1);
}

static void testBrowserInvalidLevelAndEmpty() {
    LevelCatalog catalog;
    catalog.publish("mallory", "Broken", "this is not json", 10);
    LevelBrowser browser(catalog, 10);
    browser.select(0);
    const LevelPreview pv = browser.preview();
    CHECK(!pv.valid);      // unparseable payload
    CHECK(!pv.playable()); // and therefore not playable
    CHECK(pv.title == "Broken"); // metadata still shown

    // An empty catalog: no selection, preview invalid, play fails.
    LevelCatalog emptyCatalog;
    LevelBrowser emptyBrowser(emptyCatalog);
    CHECK(emptyBrowser.empty());
    CHECK(emptyBrowser.selected() == nullptr);
    CHECK(!emptyBrowser.preview().valid);
    std::string out;
    CHECK(!emptyBrowser.play(out));
}

int main() {
    testStableContentCode();
    testBase64RoundTrip();
    testShareRoundTripAndIntegrity();
    testCrossInstanceReimport();
    testBrowserFeedsAndSelection();
    testBrowserPreviewAndPlay();
    testBrowserInvalidLevelAndEmpty();

    if (g_failures == 0) {
        std::printf("All level share / browser (UGC Phase 5) tests passed.\n");
        return 0;
    }
    std::printf("%d level share / browser test(s) failed.\n", g_failures);
    return 1;
}
