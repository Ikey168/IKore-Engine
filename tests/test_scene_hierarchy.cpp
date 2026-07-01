// Test for the scene hierarchy core - Milestone 9, #59.
//
// Verifies the headless tree model behind the ImGui hierarchy panel: add/remove
// (recursive and promoting), rename, reparent with cycle prevention, ancestor
// queries, preorder flattening, and that it tracks a live create/destroy sequence.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_scene_hierarchy.cpp -o test_scene_hierarchy

#include "ui/SceneHierarchy.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
using NodeId = SceneHierarchy::NodeId;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool contains(const std::vector<NodeId>& v, NodeId id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

static void testAddAndQuery() {
    SceneHierarchy h;
    CHECK(h.add(1, "Root A"));
    CHECK(h.add(2, "Root B"));
    CHECK(h.add(3, "Child of A", 1));
    CHECK(h.add(4, "Grandchild", 3));

    CHECK(h.size() == 4);
    CHECK(h.exists(1) && h.exists(4));
    CHECK(!h.exists(99));

    // Duplicate id and unknown parent are rejected.
    CHECK(!h.add(1, "dup"));
    CHECK(!h.add(5, "orphan", 42));
    CHECK(!h.add(SceneHierarchy::kNoNode, "sentinel"));

    CHECK(h.roots().size() == 2);
    CHECK(contains(h.roots(), 1) && contains(h.roots(), 2));
    CHECK(contains(h.children(1), 3));
    CHECK(h.children(3).size() == 1 && contains(h.children(3), 4));
    CHECK(h.parent(3) == 1);
    CHECK(h.parent(1) == SceneHierarchy::kNoNode);
    CHECK(h.name(3) == "Child of A");
    CHECK(h.name(99).empty()); // unknown id -> empty name
}

static void testRename() {
    SceneHierarchy h;
    h.add(1, "Old");
    CHECK(h.rename(1, "New"));
    CHECK(h.name(1) == "New");
    CHECK(!h.rename(99, "x"));
}

static void testAncestorAndReparent() {
    SceneHierarchy h;
    h.add(1, "A");
    h.add(2, "B", 1);
    h.add(3, "C", 2); // A > B > C

    CHECK(h.isAncestor(1, 3));
    CHECK(h.isAncestor(1, 2));
    CHECK(!h.isAncestor(3, 1));

    // Move C from under B to under A.
    CHECK(h.reparent(3, 1));
    CHECK(h.parent(3) == 1);
    CHECK(!contains(h.children(2), 3));
    CHECK(contains(h.children(1), 3));

    // Reparent to top level.
    CHECK(h.reparent(3, SceneHierarchy::kNoNode));
    CHECK(h.parent(3) == SceneHierarchy::kNoNode);
    CHECK(contains(h.roots(), 3));

    // Cycle prevention: cannot move A under its descendant B, nor under itself, nor
    // to an unknown parent. Structure must be unchanged after a rejected move.
    CHECK(!h.reparent(1, 2));
    CHECK(!h.reparent(1, 1));
    CHECK(!h.reparent(1, 424242));
    CHECK(h.parent(1) == SceneHierarchy::kNoNode);
    CHECK(contains(h.children(1), 2));

    // No-op reparent to the current parent succeeds.
    CHECK(h.reparent(2, 1));
}

static void testRemoveRecursive() {
    SceneHierarchy h;
    h.add(1, "A");
    h.add(2, "B", 1);
    h.add(3, "C", 2);
    h.add(4, "D", 1);

    CHECK(h.subtree(1).size() == 4); // A, B, C, D
    CHECK(h.subtree(2).size() == 2); // B, C (the subtree about to be removed)
    CHECK(h.remove(2, /*recursive=*/true)); // removes B and C
    CHECK(!h.exists(2));
    CHECK(!h.exists(3));
    CHECK(h.exists(1) && h.exists(4));
    CHECK(!contains(h.children(1), 2));
    CHECK(h.size() == 2);

    CHECK(!h.remove(999));
}

static void testRemovePromotingChildren() {
    SceneHierarchy h;
    h.add(1, "A");
    h.add(2, "B", 1);
    h.add(3, "C", 2);
    h.add(4, "D", 2);

    // Remove B without recursion: its children move up to A.
    CHECK(h.remove(2, /*recursive=*/false));
    CHECK(!h.exists(2));
    CHECK(h.exists(3) && h.exists(4));
    CHECK(h.parent(3) == 1);
    CHECK(h.parent(4) == 1);
    CHECK(contains(h.children(1), 3) && contains(h.children(1), 4));
    CHECK(!contains(h.children(1), 2));
}

static void testFlattenPreorder() {
    SceneHierarchy h;
    h.add(1, "A");
    h.add(2, "B", 1);
    h.add(3, "C", 1);
    h.add(4, "B1", 2);
    // A(0) > [ B(1) > B1(2), C(1) ]
    const std::vector<std::pair<NodeId, int>> flat = h.flatten();
    CHECK(flat.size() == 4);
    CHECK(flat[0].first == 1u && flat[0].second == 0);
    CHECK(flat[1].first == 2u && flat[1].second == 1);
    CHECK(flat[2].first == 4u && flat[2].second == 2);
    CHECK(flat[3].first == 3u && flat[3].second == 1);
}

static void testTracksLiveSceneChanges() {
    // Mirrors the engine adding on create and removing on destroy: the hierarchy
    // reflects the live set of entities as it changes.
    SceneHierarchy h;
    CHECK(h.size() == 0);
    for (NodeId id = 1; id <= 5; ++id) h.add(id, "E" + std::to_string(id));
    CHECK(h.size() == 5);
    CHECK(h.roots().size() == 5);

    h.remove(3);
    h.remove(5);
    CHECK(h.size() == 3);
    CHECK(!h.exists(3) && !h.exists(5));
    CHECK(h.exists(1) && h.exists(2) && h.exists(4));

    h.add(6, "E6");
    CHECK(h.size() == 4);
    CHECK(contains(h.roots(), 6));

    h.clear();
    CHECK(h.size() == 0);
    CHECK(h.roots().empty());
}

int main() {
    testAddAndQuery();
    testRename();
    testAncestorAndReparent();
    testRemoveRecursive();
    testRemovePromotingChildren();
    testFlattenPreorder();
    testTracksLiveSceneChanges();

    if (g_failures == 0) {
        std::printf("All scene hierarchy tests passed.\n");
        return 0;
    }
    std::printf("%d scene hierarchy test(s) failed.\n", g_failures);
    return 1;
}
