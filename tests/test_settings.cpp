// Test for the persistent settings store - Milestone 9, #58.
//
// Verifies typed get/set with defaults, the "key=value" serialize/load round trip
// (including comments, blank lines, and whitespace trimming), and persistence to
// disk via saveToFile/loadFromFile (settings surviving across "sessions").
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_settings.cpp -o test_settings

#include "core/Settings.h"

#include <cmath>
#include <cstdio>
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

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void testTypedGetSetAndDefaults() {
    Settings s;
    s.setBool("vsync", true);
    s.setInt("width", 1920);
    s.setFloat("volume", 0.75f);
    s.setString("profile", "player1");

    CHECK(s.getBool("vsync") == true);
    CHECK(s.getInt("width") == 1920);
    CHECK(approx(s.getFloat("volume"), 0.75f));
    CHECK(s.getString("profile") == "player1");

    // Missing keys fall back to the supplied default.
    CHECK(s.getBool("missing", true) == true);
    CHECK(s.getInt("missing", 42) == 42);
    CHECK(approx(s.getFloat("missing", 1.5f), 1.5f));
    CHECK(s.getString("missing", "def") == "def");

    CHECK(s.has("vsync"));
    CHECK(!s.has("nope"));
    CHECK(s.size() == 4);
}

static void testBoolParsingVariants() {
    Settings s;
    s.load("a=true\nb=1\nc=on\nd=yes\ne=false\nf=0\ng=nonsense\n");
    CHECK(s.getBool("a"));
    CHECK(s.getBool("b"));
    CHECK(s.getBool("c"));
    CHECK(s.getBool("d"));
    CHECK(!s.getBool("e"));
    CHECK(!s.getBool("f"));
    CHECK(!s.getBool("g")); // unrecognized -> false
}

static void testInvalidNumbersFallBack() {
    Settings s;
    s.setString("notnum", "abc");
    CHECK(s.getInt("notnum", -1) == -1);
    CHECK(approx(s.getFloat("notnum", -2.0f), -2.0f));
}

static void testSerializeRoundTrip() {
    Settings a;
    a.setBool("vsync", false);
    a.setInt("quality", 2);
    a.setFloat("volume", 0.5f);
    a.setString("name", "IKore");

    const std::string text = a.serialize();

    Settings b;
    b.load(text);
    CHECK(b.getBool("vsync") == false);
    CHECK(b.getInt("quality") == 2);
    CHECK(approx(b.getFloat("volume"), 0.5f));
    CHECK(b.getString("name") == "IKore");
    CHECK(b.size() == a.size());
}

static void testLoadMergesAndIgnoresJunk() {
    Settings s;
    s.setInt("keep", 7);       // pre-existing default that should survive
    s.setInt("quality", 0);    // will be overwritten by load
    s.load("# a comment\n"
           "; another comment\n"
           "\n"
           "  quality = 3  \n"  // whitespace around key and value is trimmed
           "volume=0.9\n"
           "malformed line without equals\n");

    CHECK(s.getInt("keep") == 7);       // untouched key survives (merge)
    CHECK(s.getInt("quality") == 3);    // overwritten, trimmed
    CHECK(approx(s.getFloat("volume"), 0.9f));
    CHECK(!s.has("malformed line without equals"));
}

static void testPersistAcrossSessions() {
    const std::string path = "ikore_settings_test.ini";

    // "Session 1" writes settings to disk.
    Settings first;
    first.setBool("fullscreen", true);
    first.setInt("width", 2560);
    first.setFloat("gamma", 1.2f);
    CHECK(first.saveToFile(path));

    // "Session 2" starts fresh and loads them back.
    Settings second;
    CHECK(second.loadFromFile(path));
    CHECK(second.getBool("fullscreen") == true);
    CHECK(second.getInt("width") == 2560);
    CHECK(approx(second.getFloat("gamma"), 1.2f));

    // Loading a non-existent file reports failure and leaves values untouched.
    Settings third;
    third.setInt("x", 5);
    CHECK(!third.loadFromFile("this_file_does_not_exist_98765.ini"));
    CHECK(third.getInt("x") == 5);

    std::remove(path.c_str());
}

int main() {
    testTypedGetSetAndDefaults();
    testBoolParsingVariants();
    testInvalidNumbersFallBack();
    testSerializeRoundTrip();
    testLoadMergesAndIgnoresJunk();
    testPersistAcrossSessions();

    if (g_failures == 0) {
        std::printf("All settings tests passed.\n");
        return 0;
    }
    std::printf("%d settings test(s) failed.\n", g_failures);
    return 1;
}
