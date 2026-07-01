// Test for the in-engine debug console core - Milestone 9, #54.
//
// Verifies the headless model behind the ImGui console: command registration and
// execution (with quoted args), scrollback, input history navigation, and
// tab-completion. The ImGui text input / panel lives in DebugUI (engine build).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_debug_console.cpp -o test_debug_console

#include "core/DebugConsole.h"

#include <cstdio>
#include <string>
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

static bool contains(const std::deque<std::string>& lines, const std::string& text) {
    for (const std::string& l : lines) {
        if (l.find(text) != std::string::npos) return true;
    }
    return false;
}

static void testExecuteAndScrollback() {
    DebugConsole c;
    c.registerCommand("echo", "echo args", [](const std::vector<std::string>& a) {
        std::string s;
        for (std::size_t i = 0; i < a.size(); ++i) s += (i ? " " : "") + a[i];
        return s;
    });
    c.registerCommand("add", "add two ints", [](const std::vector<std::string>& a) {
        if (a.size() < 2) return std::string("usage: add A B");
        return std::to_string(std::stoi(a[0]) + std::stoi(a[1]));
    });

    CHECK(c.execute("echo hello world") == "hello world");
    CHECK(c.execute("add 2 3") == "5");
    // Quoted arguments stay together.
    CHECK(c.execute("echo \"one two\" three") == "one two three");

    CHECK(contains(c.lines(), "> echo hello world")); // input is echoed
    CHECK(contains(c.lines(), "hello world"));         // output recorded

    // Unknown command reports rather than throwing.
    c.execute("bogus");
    CHECK(contains(c.lines(), "unknown command: bogus"));

    // Built-ins exist.
    CHECK(c.hasCommand("help"));
    CHECK(c.hasCommand("clear"));
}

static void testHistoryNavigation() {
    DebugConsole c;
    c.registerCommand("a", "", [](const std::vector<std::string>&) { return std::string(); });
    c.registerCommand("b", "", [](const std::vector<std::string>&) { return std::string(); });
    c.execute("a");
    c.execute("b");
    c.execute("a b"); // history: a, b, "a b"

    CHECK(c.historyPrev() == "a b");
    CHECK(c.historyPrev() == "b");
    CHECK(c.historyPrev() == "a");
    CHECK(c.historyPrev() == "a"); // clamps at the oldest
    CHECK(c.historyNext() == "b");
    CHECK(c.historyNext() == "a b");
    CHECK(c.historyNext().empty()); // past the newest -> a fresh empty line
}

static void testAutocomplete() {
    DebugConsole c;
    c.registerCommand("physics_step", "", [](const std::vector<std::string>&) { return std::string(); });
    c.registerCommand("physics_pause", "", [](const std::vector<std::string>&) { return std::string(); });
    c.registerCommand("render", "", [](const std::vector<std::string>&) { return std::string(); });

    const std::vector<std::string> phys = c.matches("phys");
    CHECK(phys.size() == 2);
    CHECK(phys[0] == "physics_pause" && phys[1] == "physics_step"); // sorted
    CHECK(c.complete("phys") == "physics_"); // longest common prefix extends the input
    CHECK(c.complete("physics_s") == "physics_step"); // unique -> full name
    CHECK(c.complete("r") == "render");      // unique among r*
    CHECK(c.matches("zzz").empty());
    CHECK(c.complete("zzz") == "zzz");       // no match -> unchanged
}

static void testClearAndMirroredLogs() {
    DebugConsole c;
    c.print("[info] engine started"); // a mirrored log line
    CHECK(contains(c.lines(), "engine started"));
    c.execute("clear");
    CHECK(c.lines().empty()); // clear command empties the scrollback
}

int main() {
    testExecuteAndScrollback();
    testHistoryNavigation();
    testAutocomplete();
    testClearAndMirroredLogs();

    if (g_failures == 0) {
        std::printf("All debug console tests passed.\n");
        return 0;
    }
    std::printf("%d debug console test(s) failed.\n", g_failures);
    return 1;
}
