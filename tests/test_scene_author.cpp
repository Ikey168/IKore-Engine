// Test for natural-language scene authoring - Milestone 13, #157.
//
// Verifies the issue's acceptance criteria:
//   - A typed prompt places coherent entities into the scene: "add a market
//     square with 8 stalls and a fountain" yields a ground slab, a central
//     fountain, and 8 stalls arranged on a ring inside the square.
//   - Generated content uses the importer/scene-graph primitives, not ad-hoc
//     code: placement is realized as a world::FloorPlan and emitted through
//     world::emitToRegistry (the M11 path), which is what creates the entities.
//   - No hard dependency on a single AI provider: an injected LLM interpreter is
//     used when present, with the offline rule-based parser as the fallback.
//
// Pure std + header-only AI / world / ECS:
//   g++ -std=c++17 -I src tests/test_scene_author.cpp -o test_scene_author

#include "ai/SceneAuthor.h"
#include "core/ecs/Registry.h"
#include "world/SvgFloorPlanImporter.h"

#include <cmath>
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

static bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

static int countKind(const std::vector<ai::Placement>& ps, const std::string& kind) {
    int n = 0;
    for (const ai::Placement& p : ps) {
        if (p.kind == kind) ++n;
    }
    return n;
}

static const ai::Placement* firstKind(const std::vector<ai::Placement>& ps, const std::string& kind) {
    for (const ai::Placement& p : ps) {
        if (p.kind == kind) return &p;
    }
    return nullptr;
}

static void testInterpretRuleBased() {
    const ai::SceneRequest req = ai::interpretPrompt("add a market square with 8 stalls and a fountain");
    CHECK(req.understood);
    CHECK(req.layout == "square");
    CHECK(req.countOf("stall") == 8);
    CHECK(req.countOf("fountain") == 1);

    // Number words and no plaza keyword (cluster layout).
    const ai::SceneRequest req2 = ai::interpretPrompt("plant three trees and two benches");
    CHECK(req2.understood);
    CHECK(req2.layout == "cluster");
    CHECK(req2.countOf("tree") == 3);
    CHECK(req2.countOf("bench") == 2);

    // Nothing recognizable.
    const ai::SceneRequest req3 = ai::interpretPrompt("the quick brown fox");
    CHECK(!req3.understood);
    CHECK(req3.objects.empty());
}

static void testAuthorPlacesCoherentScene() {
    ecs::Registry reg;
    const ai::AuthoredScene s =
        ai::authorScene("add a market square with 8 stalls and a fountain", reg);

    // One ground + 8 stalls + 1 fountain = 10 placements and 10 entities.
    CHECK(countKind(s.placements, "square") == 1);
    CHECK(countKind(s.placements, "stall") == 8);
    CHECK(countKind(s.placements, "fountain") == 1);
    CHECK(s.placements.size() == 10);
    CHECK(s.entitiesCreated == 10);
    CHECK(reg.aliveCount() == 10);

    const float ringR = ai::stallRingRadius(8);
    const float half = ringR + 2.0f;

    // Fountain sits at the center, resting on the ground (y = height/2).
    const ai::Placement* fountain = firstKind(s.placements, "fountain");
    CHECK(fountain != nullptr);
    CHECK(approx(fountain->box.center.x, 0.0f) && approx(fountain->box.center.z, 0.0f));
    CHECK(fountain->box.center.y > 0.0f);

    // Stalls are evenly on the ring (constant radius), inside the square, distinct.
    int stallsOnRing = 0, stallsInside = 0;
    float minX = 1e9f, maxX = -1e9f;
    for (const ai::Placement& p : s.placements) {
        if (p.kind != "stall") continue;
        const float r = std::sqrt(p.box.center.x * p.box.center.x + p.box.center.z * p.box.center.z);
        if (approx(r, ringR, 1e-2f)) ++stallsOnRing;
        if (std::fabs(p.box.center.x) <= half && std::fabs(p.box.center.z) <= half) ++stallsInside;
        minX = std::min(minX, p.box.center.x);
        maxX = std::max(maxX, p.box.center.x);
    }
    CHECK(stallsOnRing == 8);
    CHECK(stallsInside == 8);
    CHECK(maxX - minX > 1.0f); // they are spread out, not stacked

    // Ground is centered and large enough to contain the ring (coherent).
    const ai::Placement* ground = firstKind(s.placements, "square");
    CHECK(ground != nullptr);
    CHECK(approx(ground->box.center.x, 0.0f) && approx(ground->box.center.z, 0.0f));
    CHECK(ground->box.center.y <= 0.0f);
    CHECK(approx(ground->box.size.x, 2.0f * half) && approx(ground->box.size.z, 2.0f * half));
    CHECK(half >= ringR);
}

static void testUsesScenePrimitives() {
    // Layout produces a world::FloorPlan; emitting it via the importer path is
    // what creates the entities (no ad-hoc placement).
    const ai::AuthoredScene s =
        ai::layoutScene(ai::interpretPrompt("a plaza with 4 stalls and 2 trees"));

    const std::size_t expected = s.plan.walls.size() + (s.plan.hasFloor ? 1u : 0u);
    CHECK(s.plan.hasFloor);                       // the plaza ground
    CHECK(s.plan.walls.size() == s.placements.size() - 1); // everything except the ground

    ecs::Registry reg;
    const std::size_t created = world::emitToRegistry(s.plan, reg);
    CHECK(created == expected);
    CHECK(created == s.placements.size());
    CHECK(reg.aliveCount() == created);
}

static void testLlmInterpretAndOfflineFallback() {
    // Injected LLM provider: no real AI dependency, deterministic reply.
    ai::CompletionFn mock = [](const std::string&) {
        return std::string("layout=square; stall=6; fountain=1;");
    };
    const ai::SceneRequest req = ai::interpretPromptLlm("build me a bustling bazaar", mock);
    CHECK(req.understood);
    CHECK(req.layout == "square");
    CHECK(req.countOf("stall") == 6);
    CHECK(req.countOf("fountain") == 1);

    ecs::Registry reg;
    const ai::AuthoredScene s = ai::authorScene("build me a bustling bazaar", reg, mock);
    CHECK(s.entitiesCreated == 8); // ground + 6 stalls + 1 fountain
    CHECK(reg.aliveCount() == 8);

    // No provider attached: fall back to the offline rule-based parser.
    ecs::Registry reg2;
    const ai::AuthoredScene s2 = ai::authorScene(
        "add a market square with 8 stalls and a fountain", reg2, ai::CompletionFn{});
    CHECK(reg2.aliveCount() == 10);
    CHECK(s2.request.understood);

    // A provider that returns nothing useful also falls back.
    ai::CompletionFn confused = [](const std::string&) { return std::string("hmm, not sure"); };
    ecs::Registry reg3;
    const ai::AuthoredScene s3 =
        ai::authorScene("add a market square with 8 stalls and a fountain", reg3, confused);
    CHECK(reg3.aliveCount() == 10);
}

int main() {
    testInterpretRuleBased();
    testAuthorPlacesCoherentScene();
    testUsesScenePrimitives();
    testLlmInterpretAndOfflineFallback();

    if (g_failures == 0) {
        std::printf("All scene-authoring tests passed.\n");
        return 0;
    }
    std::printf("%d scene-authoring test(s) failed.\n", g_failures);
    return 1;
}
