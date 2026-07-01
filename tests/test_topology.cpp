// Test for room/doorway/connectivity topology - Milestone 16, #168.
//
// Verifies the issue's acceptance criterion: rooms, doors, and connectivity are
// correctly derived from the vectorized walls. Uses hand-built wall grids (# wall,
// . open) for precise checks, plus a rasterized-polyline wrapper.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_topology.cpp -o test_topology

#include "cv/Topology.h"
#include "cv/Vectorize.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace IKore::cv;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static Mask maskFromRows(const std::vector<std::string>& rows) {
    const int h = static_cast<int>(rows.size());
    const int w = static_cast<int>(rows[0].size());
    Mask m(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#') m.set(x, y, 1);
        }
    }
    return m;
}

static void testTwoRoomsEntranceAndConnectivity() {
    // A building split into a left and right room by an inner wall with a doorway,
    // plus an entrance gap in the left outer wall leading outside.
    const Mask walls = maskFromRows({
        "............",
        "............",
        "..########..",
        "..#..#...#..",
        "..#..#...#..",
        ".........#..", // entrance (col2) + inner doorway (col5), right outer wall col9
        "..#..#...#..",
        "..#..#...#..",
        "..#..#...#..",
        "..########..",
        "............",
        "............",
    });
    const Topology t = buildTopologyFromGrid(walls);

    CHECK(t.rooms.size() == 3); // outside + two interior rooms
    CHECK(t.interiorRoomCount() == 2);
    CHECK(t.doorways.size() == 2);
    CHECK(t.ambiguousGaps == 0);

    // Identify rooms: outside, left (area 12), right (area 18).
    int outside = -1, left = -1, right = -1;
    for (const Room& r : t.rooms) {
        if (r.isOutside) outside = r.id;
        else if (r.area == 12) left = r.id;
        else if (r.area == 18) right = r.id;
    }
    CHECK(outside >= 0 && left >= 0 && right >= 0);

    // The inner doorway links the two rooms; the entrance links the left room out.
    CHECK(t.connected(left, right));
    CHECK(t.connected(outside, left));
    CHECK(!t.connected(outside, right));   // no direct doorway
    CHECK(t.reachable(outside, right));     // but reachable via the left room

    int toOutsideDoors = 0;
    for (const Doorway& d : t.doorways) {
        if (d.toOutside) ++toOutsideDoors;
    }
    CHECK(toOutsideDoors == 1); // just the entrance
}

static void testAmbiguousGapFlagged() {
    // A stray wall stub with a gap whose two sides are the same region: not a real
    // doorway, so it is flagged as ambiguous.
    const Mask walls = maskFromRows({
        ".......",
        "...#...",
        "...#...",
        ".......", // gap at col3
        "...#...",
        "...#...",
        ".......",
    });
    const Topology t = buildTopologyFromGrid(walls);
    CHECK(t.rooms.size() == 1);        // one open region (outside)
    CHECK(t.interiorRoomCount() == 0);
    CHECK(t.doorways.empty());          // the gap does not join two distinct rooms
    CHECK(t.ambiguousGaps == 1);
}

static void testClosedBoxFromPolylines() {
    // A closed rectangle of walls -> exactly one enclosed room, no doorways.
    const std::vector<Polyline> walls = {
        {{10, 10}, {40, 10}, {40, 40}, {10, 40}, {10, 10}},
    };
    const Topology t = buildTopology(walls, 10.0f, 1);
    CHECK(t.interiorRoomCount() == 1);
    CHECK(t.doorways.empty());
    CHECK(t.ambiguousGaps == 0);
}

int main() {
    testTwoRoomsEntranceAndConnectivity();
    testAmbiguousGapFlagged();
    testClosedBoxFromPolylines();

    if (g_failures == 0) {
        std::printf("All topology tests passed.\n");
        return 0;
    }
    std::printf("%d topology test(s) failed.\n", g_failures);
    return 1;
}
