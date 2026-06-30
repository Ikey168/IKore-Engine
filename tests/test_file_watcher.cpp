// Test for the hot-reload file watcher - Milestone 10, issue #146.
//
// Verifies the issue's acceptance criteria at the mechanism level:
//   - Editing a watched file triggers its reload callback at runtime.
//   - A reload callback that fails is reported and does NOT crash; polling
//     continues for the other watched files.
// Plus: no spurious fire on an unchanged file, missing-then-created files, and
// unwatch. Pure std::filesystem, so it builds and runs standalone:
//   g++ -std=c++17 -I src tests/test_file_watcher.cpp -o test_file_watcher

#include "core/FileWatcher.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace IKore;
namespace fs = std::filesystem;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static void writeFile(const fs::path& p, const std::string& contents) {
    std::ofstream out(p, std::ios::trunc);
    out << contents;
}

// Make a file look freshly edited, deterministically (avoids mtime-granularity
// flakiness from a real write).
static void bumpMtime(const fs::path& p) {
    std::error_code ec;
    fs::last_write_time(p, fs::file_time_type::clock::now() + std::chrono::seconds(2), ec);
}

int main() {
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "ikore_filewatcher_test";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    const fs::path scriptFile = dir / "script.lua";
    const fs::path badFile = dir / "bad.txt";
    const fs::path lateFile = dir / "late.txt";

    writeFile(scriptFile, "v1");
    writeFile(badFile, "x");

    FileWatcher watcher;
    int errorCount = 0;
    watcher.setErrorHandler([&errorCount](const std::string&, const std::string&) { ++errorCount; });

    int scriptReloads = 0;
    std::string lastContents;
    const std::size_t scriptId = watcher.watch(scriptFile.string(), [&](const std::string& path) {
        ++scriptReloads;
        std::ifstream in(path);
        std::getline(in, lastContents);
    });

    int badReloads = 0;
    watcher.watch(badFile.string(), [&badReloads](const std::string&) {
        ++badReloads;
        throw std::runtime_error("simulated reload failure");
    });

    int lateReloads = 0;
    watcher.watch(lateFile.string(), [&lateReloads](const std::string&) { ++lateReloads; });

    CHECK(watcher.count() == 3);

    // 1) No change since watch() -> nothing fires.
    watcher.poll();
    CHECK(scriptReloads == 0);
    CHECK(errorCount == 0);

    // 2) Edit the script -> its callback fires once with the new contents.
    writeFile(scriptFile, "v2");
    bumpMtime(scriptFile);
    watcher.poll();
    CHECK(scriptReloads == 1);
    CHECK(lastContents == "v2");

    // 3) Polling again with no change -> no extra fire.
    watcher.poll();
    CHECK(scriptReloads == 1);

    // 4) A failing reload is reported and does not crash; other files still poll.
    bumpMtime(badFile);
    writeFile(scriptFile, "v3");
    bumpMtime(scriptFile);
    watcher.poll();
    CHECK(badReloads == 1);       // the bad callback ran
    CHECK(errorCount == 1);       // and its failure was reported
    CHECK(scriptReloads == 2);    // poll still serviced the good file
    CHECK(lastContents == "v3");

    // 5) A file that did not exist at watch() time fires once it appears.
    watcher.poll();
    CHECK(lateReloads == 0);      // still missing
    writeFile(lateFile, "hello");
    watcher.poll();
    CHECK(lateReloads == 1);

    // 6) After unwatch, edits no longer fire.
    CHECK(watcher.unwatch(scriptId));
    writeFile(scriptFile, "v4");
    bumpMtime(scriptFile);
    watcher.poll();
    CHECK(scriptReloads == 2);    // unchanged from before

    fs::remove_all(dir, ec);

    if (g_failures == 0) {
        std::printf("All file watcher (hot reload) tests passed.\n");
        return 0;
    }
    std::printf("%d file watcher test(s) failed.\n", g_failures);
    return 1;
}
