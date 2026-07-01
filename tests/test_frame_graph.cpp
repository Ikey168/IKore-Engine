// Test for the concrete frame graph + executor - issue #232 (rendering P1 adoption).
//
// Verifies that the renderer's real pass set (buildDefaultFrameGraph) resolves,
// is acyclic, and orders to exactly the sequence the renderer used before the
// graph was wired in - so adoption cannot cause a visual regression - and that
// FrameGraphExecutor runs handlers in that order and skips unwired passes.
//
// Pure std + header-only (no GL context):
//   g++ -std=c++17 -I src tests/test_frame_graph.cpp -o test_frame_graph

#include "render/FrameGraph.h"

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

// The default frame graph is well-formed: all six passes, deps resolve, acyclic.
static void testDefaultFrameGraphIsWellFormed() {
    const RenderPassGraph g = buildDefaultFrameGraph();
    CHECK(g.passCount() == 6);
    CHECK(g.hasPass(passes::Shadow));
    CHECK(g.hasPass(passes::PostProcess));
    CHECK(g.dependenciesResolved());
    CHECK(!g.hasCycle());
}

// It must reproduce the exact pre-adoption order so no scene renders differently.
static void testDefaultFrameGraphOrderIsStable() {
    const std::vector<std::string> expected = {
        passes::Shadow, passes::SceneBegin, passes::Skybox,
        passes::Forward, passes::Particles, passes::PostProcess,
    };
    CHECK(buildDefaultFrameGraph().order() == expected);
}

// The real data-flow constraints hold regardless of tie-break details.
static void testDependencyConstraintsHold() {
    const std::vector<std::string> order = buildDefaultFrameGraph().order();
    CHECK(indexOf(order, passes::Shadow) < indexOf(order, passes::Forward));
    CHECK(indexOf(order, passes::SceneBegin) < indexOf(order, passes::Skybox));
    CHECK(indexOf(order, passes::Skybox) < indexOf(order, passes::Forward));
    CHECK(indexOf(order, passes::Forward) < indexOf(order, passes::Particles));
    CHECK(indexOf(order, passes::Particles) < indexOf(order, passes::PostProcess));
    CHECK(order.back() == passes::PostProcess);
}

// The executor computes the same order and invokes handlers in it.
static void testExecutorRunsHandlersInOrder() {
    FrameGraphExecutor exec(buildDefaultFrameGraph());
    const std::vector<std::string> expected = {
        passes::Shadow, passes::SceneBegin, passes::Skybox,
        passes::Forward, passes::Particles, passes::PostProcess,
    };
    CHECK(exec.order() == expected);

    std::vector<std::string> ran;
    for (const std::string& p : expected) {
        exec.setHandler(p, [p, &ran]() { ran.push_back(p); });
    }
    exec.execute();
    CHECK(ran == expected);
}

// A pass with no handler is a no-op, so passes can be wired up incrementally.
static void testUnwiredPassesAreSkipped() {
    FrameGraphExecutor exec(buildDefaultFrameGraph());
    int forwardRuns = 0;
    exec.setHandler(passes::Forward, [&forwardRuns]() { ++forwardRuns; });
    exec.execute();
    CHECK(forwardRuns == 1); // only the one wired pass ran; the other five were skipped
}

int main() {
    testDefaultFrameGraphIsWellFormed();
    testDefaultFrameGraphOrderIsStable();
    testDependencyConstraintsHold();
    testExecutorRunsHandlersInOrder();
    testUnwiredPassesAreSkipped();

    if (g_failures == 0) {
        std::printf("All frame graph tests passed.\n");
        return 0;
    }
    std::printf("%d frame graph test(s) failed.\n", g_failures);
    return 1;
}
