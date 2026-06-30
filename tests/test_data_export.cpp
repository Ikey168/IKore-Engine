// Test for per-tick simulation data export - Milestone 12, #155.
//
// Verifies the issue's acceptance criteria:
//   - A run produces a structured, parseable data file: a real crowd run is
//     exported to CSV and JSON; the CSV is parsed back manually and the JSON is
//     round-tripped through the engine's own JSON parser, and both reproduce the
//     simulated values.
//   - Negligible impact when disabled: a disabled exporter, used with the
//     recommended enabled() guard, builds and writes nothing (zero work).
//
// Pure std + header-only sim / ECS / nav / JSON parser:
//   g++ -std=c++17 -I src tests/test_data_export.cpp -o test_data_export

#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"
#include "core/sim/Crowd.h"
#include "core/sim/DataExport.h"
#include "world/GeoJsonImporter.h" // reuse its self-contained JSON parser
#include "world/NavMesh.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
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

static bool approx(double a, double b, double eps = 1e-3) {
    return std::fabs(a - b) <= eps;
}

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : s) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

static std::vector<std::string> splitCommas(const std::string& s) {
    std::vector<std::string> fields;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

static void testCsvFormat() {
    std::ostringstream out;
    sim::DataExporter ex;
    ex.begin(out, sim::ExportFormat::Csv, {"tick", "id", "x"});
    CHECK(ex.enabled());
    ex.record({0, 0, 1.5});
    ex.record({1, 2, 0.25});
    ex.record({2, 3, 4.0}); // integral value prints without a decimal point
    ex.end();
    CHECK(!ex.enabled());
    CHECK(ex.rowCount() == 3);

    CHECK(out.str() == "tick,id,x\n0,0,1.5\n1,2,0.25\n2,3,4\n");
}

static void testJsonRoundTrip() {
    std::ostringstream out;
    sim::DataExporter ex;
    ex.begin(out, sim::ExportFormat::Json, {"tick", "id", "x"});
    ex.record({0, 0, 1.5});
    ex.record({1, 2, 0.25});
    ex.end();

    // Parse it back with the engine's own JSON parser: it must be a valid array
    // of objects with the right keys and values.
    world::detail::Json root = world::detail::JsonParser(out.str()).parse();
    CHECK(root.type == world::detail::Json::Type::Arr);
    CHECK(root.size() == 2);
    CHECK(root[0].has("tick") && root[0].has("id") && root[0].has("x"));
    CHECK(approx(root[0].at("tick").asNumber(), 0.0));
    CHECK(approx(root[0].at("id").asNumber(), 0.0));
    CHECK(approx(root[0].at("x").asNumber(), 1.5));
    CHECK(approx(root[1].at("tick").asNumber(), 1.0));
    CHECK(approx(root[1].at("id").asNumber(), 2.0));
    CHECK(approx(root[1].at("x").asNumber(), 0.25));

    // An export with no records is still a valid (empty) JSON array.
    std::ostringstream empty;
    sim::DataExporter ex2;
    ex2.begin(empty, sim::ExportFormat::Json, {"a"});
    ex2.end();
    world::detail::Json emptyRoot = world::detail::JsonParser(empty.str()).parse();
    CHECK(emptyRoot.type == world::detail::Json::Type::Arr);
    CHECK(emptyRoot.size() == 0);
}

// Export a real crowd run and confirm both formats reproduce the simulated data.
static void testCrowdRunProducesParseableData() {
    const world::NavGrid grid = world::bake(0.0f, 0.0f, 16.0f, 16.0f, 1.0f, {});
    const ecs::Vec3 goal{14.5f, 0.0f, 14.5f};
    const sim::FlowField field = sim::buildFlowField(grid, goal);

    ecs::Registry reg;
    std::vector<ecs::Entity> agents;
    for (int i = 0; i < 6; ++i) {
        agents.push_back(sim::spawnAgent(reg, grid.cellCenter(i + 1, 1), 6.0f, 0.3f));
    }

    std::ostringstream csvOut, jsonOut;
    sim::DataExporter csv, json;
    const std::vector<std::string> cols = {"tick", "id", "x", "z"};
    csv.begin(csvOut, sim::ExportFormat::Csv, cols);
    json.begin(jsonOut, sim::ExportFormat::Json, cols);

    std::vector<std::array<double, 4>> expected;
    const int ticks = 5;
    for (int t = 0; t < ticks; ++t) {
        sim::steerCrowd(reg, grid, field, 0.1f);
        for (std::size_t id = 0; id < agents.size(); ++id) {
            const ecs::Transform& tr = reg.get<ecs::Transform>(agents[id]);
            const std::array<double, 4> row = {static_cast<double>(t), static_cast<double>(id),
                                               tr.position.x, tr.position.z};
            const std::vector<double> v(row.begin(), row.end());
            if (csv.enabled()) csv.record(v);
            if (json.enabled()) json.record(v);
            expected.push_back(row);
        }
    }
    csv.end();
    json.end();

    const std::size_t rows = expected.size();
    CHECK(rows == static_cast<std::size_t>(ticks) * agents.size());

    // --- CSV: header + one row per record, values reproduce the simulation. ---
    const std::vector<std::string> lines = splitLines(csvOut.str());
    CHECK(lines.size() == rows + 1);
    CHECK(lines[0] == "tick,id,x,z");
    bool csvMatches = true;
    for (std::size_t r = 0; r < rows; ++r) {
        const std::vector<std::string> f = splitCommas(lines[r + 1]);
        if (f.size() != 4) {
            csvMatches = false;
            continue;
        }
        if (!approx(std::strtod(f[0].c_str(), nullptr), expected[r][0]) ||
            !approx(std::strtod(f[1].c_str(), nullptr), expected[r][1]) ||
            !approx(std::strtod(f[2].c_str(), nullptr), expected[r][2]) ||
            !approx(std::strtod(f[3].c_str(), nullptr), expected[r][3])) {
            csvMatches = false;
        }
    }
    CHECK(csvMatches);

    // --- JSON: a valid array of objects that reproduces the simulation. ---
    world::detail::Json root = world::detail::JsonParser(jsonOut.str()).parse();
    CHECK(root.type == world::detail::Json::Type::Arr);
    CHECK(root.size() == rows);
    bool jsonMatches = true;
    for (std::size_t r = 0; r < rows && r < root.size(); ++r) {
        const world::detail::Json& o = root[r];
        if (!o.has("tick") || !o.has("id") || !o.has("x") || !o.has("z")) {
            jsonMatches = false;
            continue;
        }
        if (!approx(o.at("tick").asNumber(), expected[r][0]) ||
            !approx(o.at("id").asNumber(), expected[r][1]) ||
            !approx(o.at("x").asNumber(), expected[r][2]) ||
            !approx(o.at("z").asNumber(), expected[r][3])) {
            jsonMatches = false;
        }
    }
    CHECK(jsonMatches);
}

static void testDisabledIsNegligible() {
    // A default-constructed exporter is disabled.
    sim::DataExporter ex;
    CHECK(!ex.enabled());

    // With the recommended enabled() guard, the row is never even built.
    int rowsBuilt = 0;
    auto buildRow = [&]() {
        ++rowsBuilt;
        return std::vector<double>{1.0, 2.0, 3.0};
    };
    const int kCalls = 100000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCalls; ++i) {
        if (ex.enabled()) ex.record(buildRow());
    }
    const auto t1 = std::chrono::steady_clock::now();
    CHECK(rowsBuilt == 0);        // guarded call site did zero work
    CHECK(ex.rowCount() == 0);

    // Calling record() directly on a disabled exporter is also a safe no-op.
    ex.record({1.0, 2.0, 3.0});
    CHECK(ex.rowCount() == 0);

    // Enabled cost, for contrast (informational).
    std::ostringstream out;
    sim::DataExporter on;
    on.begin(out, sim::ExportFormat::Csv, {"a", "b", "c"});
    const auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCalls; ++i) {
        if (on.enabled()) on.record(buildRow());
    }
    const auto t3 = std::chrono::steady_clock::now();
    on.end();
    CHECK(on.rowCount() == static_cast<std::size_t>(kCalls));

    const double offMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double onMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
    std::printf("[info] %d export calls: disabled %.3f ms (0 rows built), enabled %.3f ms\n",
                kCalls, offMs, onMs);
}

int main() {
    testCsvFormat();
    testJsonRoundTrip();
    testCrowdRunProducesParseableData();
    testDisabledIsNegligible();

    if (g_failures == 0) {
        std::printf("All data-export tests passed.\n");
        return 0;
    }
    std::printf("%d data-export test(s) failed.\n", g_failures);
    return 1;
}
