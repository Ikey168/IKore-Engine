// Test for the render-pass dependency graph - issue #226, step 1.
//
// Verifies deterministic topological ordering (dependencies before dependents),
// stable insertion-order tie-breaking, dependency resolution checks, and cycle
// detection. This core is not yet wired into the GL renderer.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_render_pass_graph.cpp -o test_render_pass_graph

#include "render/RenderPassGraph.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace IKore::render;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static int indexOf(const std::vector<std::string>& order, const std::string& name) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == name) return static_cast<int>(i);
    }
    return -1;
}

static void testLinearChain() {
    RenderPassGraph g;
    g.addPass("a");
    g.addPass("b", {"a"});
    g.addPass("c", {"b"});

    CHECK(g.passCount() == 3);
    CHECK(g.hasPass("b") && !g.hasPass("z"));
    CHECK(g.dependenciesResolved());
    CHECK(!g.hasCycle());

    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 3);
    CHECK(order[0] == "a" && order[1] == "b" && order[2] == "c");
}

static void testIndependentPassesKeepInsertionOrder() {
    RenderPassGraph g;
    g.addPass("first");
    g.addPass("second");
    g.addPass("third");
    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 3);
    CHECK(order[0] == "first" && order[1] == "second" && order[2] == "third");
}

static void testDiamond() {
    RenderPassGraph g;
    g.addPass("root");
    g.addPass("left", {"root"});
    g.addPass("right", {"root"});
    g.addPass("merge", {"left", "right"});

    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 4);
    CHECK(indexOf(order, "root") < indexOf(order, "left"));
    CHECK(indexOf(order, "root") < indexOf(order, "right"));
    CHECK(indexOf(order, "left") < indexOf(order, "merge"));
    CHECK(indexOf(order, "right") < indexOf(order, "merge"));
    // Deterministic tie-break: left was added before right.
    CHECK(indexOf(order, "left") < indexOf(order, "right"));
}

static void testDependencyDeclaredBeforeItsProducer() {
    // A pass may list a dependency that is registered later; ordering still holds.
    RenderPassGraph g;
    g.addPass("consumer", {"producer"});
    g.addPass("producer");
    CHECK(g.dependenciesResolved());
    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 2);
    CHECK(indexOf(order, "producer") < indexOf(order, "consumer"));
}

static void testUnresolvedDependency() {
    RenderPassGraph g;
    g.addPass("lighting", {"gbuffer"}); // gbuffer never registered
    CHECK(!g.dependenciesResolved());
    // An unregistered dependency imposes no constraint, so ordering still succeeds.
    CHECK(!g.hasCycle());
    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 1 && order[0] == "lighting");
}

static void testCycleDetection() {
    RenderPassGraph g;
    g.addPass("a", {"b"});
    g.addPass("b", {"a"});
    CHECK(g.hasCycle());
    CHECK(g.order().empty());

    RenderPassGraph selfLoop;
    selfLoop.addPass("x", {"x"});
    CHECK(selfLoop.hasCycle());
}

static void testRealisticPipeline() {
    // A forward pipeline resembling the engine's passes.
    RenderPassGraph g;
    g.addPass("shadow");
    g.addPass("gbuffer");
    g.addPass("lighting", {"gbuffer", "shadow"});
    g.addPass("bloom", {"lighting"});
    g.addPass("ssao", {"gbuffer"});
    g.addPass("composite", {"lighting", "bloom", "ssao"});
    g.addPass("fxaa", {"composite"});
    g.addPass("present", {"fxaa"});

    CHECK(g.dependenciesResolved());
    CHECK(!g.hasCycle());
    const std::vector<std::string> order = g.order();
    CHECK(order.size() == 8);
    CHECK(indexOf(order, "shadow") < indexOf(order, "lighting"));
    CHECK(indexOf(order, "gbuffer") < indexOf(order, "lighting"));
    CHECK(indexOf(order, "gbuffer") < indexOf(order, "ssao"));
    CHECK(indexOf(order, "lighting") < indexOf(order, "composite"));
    CHECK(indexOf(order, "bloom") < indexOf(order, "composite"));
    CHECK(indexOf(order, "ssao") < indexOf(order, "composite"));
    CHECK(indexOf(order, "composite") < indexOf(order, "fxaa"));
    CHECK(order.back() == "present");
}

int main() {
    testLinearChain();
    testIndependentPassesKeepInsertionOrder();
    testDiamond();
    testDependencyDeclaredBeforeItsProducer();
    testUnresolvedDependency();
    testCycleDetection();
    testRealisticPipeline();

    if (g_failures == 0) {
        std::printf("All render pass graph tests passed.\n");
        return 0;
    }
    std::printf("%d render pass graph test(s) failed.\n", g_failures);
    return 1;
}
