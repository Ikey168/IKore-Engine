// Test for the IBrain interface + LLM/behavior-tree NPCs - Milestone 13, #156.
//
// Verifies the issue's acceptance criteria:
//   - NPCs pursue goals and react to world events: the behavior-tree brain moves
//     toward the goal, idles at it, and flees a perceived threat; an NPC wired to
//     the EventSystem consumes a published threat and emits its action back.
//   - No hard dependency on any single AI provider: the LLM brain runs through an
//     injected completion function (a mock here, no provider linked), and when no
//     provider is available the FallbackBrain transparently uses the offline
//     behavior tree.
//
// Links the engine EventSystem (header-only AI + EventSystem.cpp):
//   g++ -std=c++17 -I src tests/test_ai_brain.cpp src/core/EventSystem.cpp -o test_ai_brain

#include "ai/BehaviorTree.h"
#include "ai/LlmBrain.h"
#include "ai/Npc.h"
#include "core/EventSystem.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

using namespace IKore;
using namespace IKore::ai;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static BrainContext makeCtx(const ecs::Vec3& self, const ecs::Vec3& goal,
                            const std::string& goalTag = "exit") {
    BrainContext ctx;
    ctx.npcName = "guard";
    ctx.goal = goalTag;
    ctx.selfPosition = self;
    ctx.goalPosition = goal;
    return ctx;
}

static void testBehaviorTreePursuesAndReacts() {
    std::unique_ptr<IBrain> brain = makeDefaultNpcBrain();

    // Far from the goal, nothing perceived: pursue the goal.
    {
        BrainContext ctx = makeCtx({0, 0, 0}, {10, 0, 0});
        const Decision d = brain->think(ctx);
        CHECK(d.action == "move_to");
        CHECK(d.target == "exit");
        CHECK(std::fabs(d.position.x - 10.0f) < 1e-3f);
        CHECK(d.valid);
    }

    // Standing on the goal: idle.
    {
        BrainContext ctx = makeCtx({10, 0, 0}, {10, 0, 0});
        const Decision d = brain->think(ctx);
        CHECK(d.action == "idle");
    }

    // A perceived threat overrides goal-seeking: flee away from it.
    {
        BrainContext ctx = makeCtx({0, 0, 0}, {10, 0, 0});
        ctx.percepts.push_back(Percept{"threat", "wolf", 1.0f, {1, 0, 0}});
        const Decision d = brain->think(ctx);
        CHECK(d.action == "flee");
        CHECK(d.target == "wolf");
        CHECK(d.position.x < 0.0f); // moved away from the threat at +x
    }
}

static void testMemoryIsBounded() {
    Memory m(3);
    m.remember("saw:a");
    m.remember("saw:b");
    m.remember("saw:c");
    m.remember("saw:d"); // evicts the oldest
    CHECK(m.size() == 3);
    CHECK(!m.recalls("saw:a"));
    CHECK(m.recalls("saw:b"));
    CHECK(m.recalls("saw:d"));
}

static void testLlmBrainParsesReply() {
    // A mock provider: no real AI dependency, deterministic reply.
    LlmBrain brain([](const std::string&) {
        return std::string("{\"action\":\"flee\",\"target\":\"wolf\",\"rationale\":\"scared\"}");
    });
    CHECK(brain.usable());

    BrainContext ctx = makeCtx({0, 0, 0}, {10, 0, 0});
    ctx.percepts.push_back(Percept{"threat", "wolf", 1.0f, {1, 0, 0}});
    Memory mem;
    mem.remember("heard:howl");
    ctx.memory = &mem;

    const Decision d = brain.think(ctx);
    CHECK(d.valid);
    CHECK(d.action == "flee");
    CHECK(d.target == "wolf");
    CHECK(d.rationale == "scared");

    // The prompt should carry the goal, the percept, and the memory.
    const std::string prompt = brain.buildPrompt(ctx);
    CHECK(prompt.find("exit") != std::string::npos);
    CHECK(prompt.find("wolf") != std::string::npos);
    CHECK(prompt.find("heard:howl") != std::string::npos);
}

static void testParseDecisionTolerance() {
    // Prose around the JSON is fine.
    Decision d;
    CHECK(LlmBrain::parseDecision(
        "Sure! My decision: {\"action\": \"move_to\", \"target\": \"exit\"} - good luck", d));
    CHECK(d.action == "move_to");
    CHECK(d.target == "exit");

    // No action field -> cannot parse.
    Decision bad;
    CHECK(!LlmBrain::parseDecision("{\"target\":\"exit\"}", bad));
}

static void testFallbackWorksOffline() {
    // Primary LLM brain with NO provider attached (offline) + behavior-tree
    // fallback: the NPC must still pursue goals and react to events.
    FallbackBrain fb(std::unique_ptr<IBrain>(new LlmBrain(CompletionFn{})), makeDefaultNpcBrain());

    BrainContext pursue = makeCtx({0, 0, 0}, {10, 0, 0});
    Decision d = fb.think(pursue);
    CHECK(d.action == "move_to");
    CHECK(!fb.lastUsedPrimary()); // fell back to the offline tree

    BrainContext react = makeCtx({0, 0, 0}, {10, 0, 0});
    react.percepts.push_back(Percept{"threat", "wolf", 1.0f, {1, 0, 0}});
    d = fb.think(react);
    CHECK(d.action == "flee");
    CHECK(!fb.lastUsedPrimary());
}

static void testFallbackPrefersLlmWhenAvailable() {
    // A working provider: the LLM decision is used.
    auto good = std::unique_ptr<IBrain>(new LlmBrain(
        [](const std::string&) { return std::string("{\"action\":\"interact\",\"target\":\"lever\"}"); }));
    FallbackBrain fb(std::move(good), makeDefaultNpcBrain());
    BrainContext ctx = makeCtx({0, 0, 0}, {10, 0, 0});
    Decision d = fb.think(ctx);
    CHECK(d.action == "interact");
    CHECK(d.target == "lever");
    CHECK(fb.lastUsedPrimary());

    // A provider that returns garbage -> unparseable -> fall back to the tree.
    auto bad = std::unique_ptr<IBrain>(
        new LlmBrain([](const std::string&) { return std::string("I am not sure what to do."); }));
    FallbackBrain fb2(std::move(bad), makeDefaultNpcBrain());
    d = fb2.think(ctx);
    CHECK(d.action == "move_to"); // tree pursued the goal
    CHECK(!fb2.lastUsedPrimary());
}

static void testNpcConsumesAndEmitsEvents() {
    EventSystem& es = EventSystem::getInstance();
    es.clear();

    Npc npc("guard", makeDefaultNpcBrain());
    npc.setGoal("exit", {10, 0, 0});
    npc.setPosition({0, 0, 0});

    NpcEventBridge bridge(npc, es);
    bridge.listen("threat"); // consume threats from the EventSystem

    // Capture the NPC's emitted actions.
    std::string emittedAction;
    EventSubscription actionSub = es.subscribe("npc.action", [&](const std::shared_ptr<EventData>& d) {
        if (auto* td = dynamic_cast<TypedEventData<Decision>*>(d.get())) {
            emittedAction = td->data.action;
        }
    });

    // Publish a world event: the NPC consumes it as a percept and remembers it.
    es.publish<Percept>("threat", Percept{"threat", "wolf", 1.0f, {1, 0, 0}});
    CHECK(npc.pendingPercepts() == 1);
    CHECK(npc.memory().recalls("wolf"));

    // Decide + emit: it reacts to the consumed threat and publishes the action.
    Decision d = bridge.step();
    CHECK(d.action == "flee");
    CHECK(emittedAction == "flee"); // the action was emitted as an event
    CHECK(npc.pendingPercepts() == 0);

    // With nothing perceived, it pursues the goal and emits that.
    emittedAction.clear();
    d = bridge.step();
    CHECK(d.action == "move_to");
    CHECK(emittedAction == "move_to");

    es.unsubscribe(actionSub);
    es.clear();
}

int main() {
    testBehaviorTreePursuesAndReacts();
    testMemoryIsBounded();
    testLlmBrainParsesReply();
    testParseDecisionTolerance();
    testFallbackWorksOffline();
    testFallbackPrefersLlmWhenAvailable();
    testNpcConsumesAndEmitsEvents();

    if (g_failures == 0) {
        std::printf("All AI brain (IBrain + LLM/BT NPC) tests passed.\n");
        return 0;
    }
    std::printf("%d AI brain test(s) failed.\n", g_failures);
    return 1;
}
