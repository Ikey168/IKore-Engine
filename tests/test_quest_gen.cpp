// Test for procedural quests & dialogue from world state - Milestone 13, #158.
//
// Verifies the issue's acceptance criteria:
//   - Quests/dialogue reference real world state: the quest's target, location,
//     and referenced event all come from the supplied WorldState, and the steps
//     mention them. An LLM target/location that is not in the world is rejected.
//   - Output is consistent with NPC memory and goals: the verb and target derive
//     from the goal, and the dialogue references a remembered subject that is
//     itself grounded in world state.
//
// Pure std + header-only AI layer:
//   g++ -std=c++17 -I src tests/test_quest_gen.cpp -o test_quest_gen

#include "ai/BehaviorTree.h"
#include "ai/Npc.h"
#include "ai/QuestGen.h"

#include <cstdio>
#include <string>
#include <vector>

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

static bool anyContains(const std::vector<std::string>& v, const std::string& sub) {
    for (const std::string& s : v) {
        if (s.find(sub) != std::string::npos) return true;
    }
    return false;
}

static WorldState sampleWorld() {
    WorldState w;
    w.locations = {"market square", "north gate"};
    w.entities = {"artifact", "fountain", "thief"};
    w.events = {"a theft at the market square"};
    return w;
}

static QuestContext sampleContext(const Memory& mem) {
    QuestContext ctx;
    ctx.npcName = "Elara";
    ctx.goal = "retrieve the artifact";
    ctx.memory = &mem;
    ctx.world = sampleWorld();
    return ctx;
}

static void testQuestReferencesRealWorldState() {
    Memory mem;
    mem.remember("saw:thief");
    const QuestContext ctx = sampleContext(mem);

    const Quest q = generateQuest(ctx);
    CHECK(q.valid);

    // Everything the quest names is real world state.
    CHECK(ctx.world.hasEntity(q.targetEntity));
    CHECK(ctx.world.hasLocation(q.location));
    CHECK(WorldState::contains(ctx.world.events, q.referencedEvent));

    // The steps actually mention the grounded target, location, and event.
    CHECK(anyContains(q.steps, q.targetEntity));
    CHECK(anyContains(q.steps, q.location));
    CHECK(anyContains(q.steps, q.referencedEvent));
}

static void testConsistentWithGoalAndMemory() {
    Memory mem;
    mem.remember("saw:thief");
    const QuestContext ctx = sampleContext(mem);

    const Quest q = generateQuest(ctx);
    // Verb and target derive from the goal ("retrieve the artifact").
    CHECK(q.verb == "recover");
    CHECK(q.targetEntity == "artifact");

    const Dialogue d = generateDialogue(ctx, q);
    CHECK(d.valid);
    CHECK(d.speaker == "Elara");
    // Dialogue states the goal-derived objective (target + real location)...
    CHECK(anyContains(d.lines, "artifact"));
    CHECK(anyContains(d.lines, "market square"));
    // ...and references a remembered subject grounded in the world (the thief).
    CHECK(anyContains(d.lines, "thief"));
}

static void testGroundingRejectsUngrounded() {
    Memory mem;
    const QuestContext ctx = sampleContext(mem);

    // No entities or no locations -> cannot ground -> invalid.
    QuestContext noEntities = ctx;
    noEntities.world.entities.clear();
    CHECK(!generateQuest(noEntities).valid);

    QuestContext noLocations = ctx;
    noLocations.world.locations.clear();
    CHECK(!generateQuest(noLocations).valid);

    // An LLM that references things not in the world is rejected.
    CompletionFn hallucinating = [](const std::string&) {
        return std::string("target=dragon; location=castle; verb=slay;");
    };
    CHECK(!generateQuestLlm(ctx, hallucinating).valid);

    // authorQuest therefore falls back to the grounded rule-based quest.
    const Quest q = authorQuest(ctx, hallucinating);
    CHECK(q.valid);
    CHECK(ctx.world.hasEntity(q.targetEntity));
    CHECK(ctx.world.hasLocation(q.location));
}

static void testBuiltFromNpc() {
    // Build the context straight from an NPC's name, goal, and memory.
    Npc npc("Elara", makeDefaultNpcBrain());
    npc.setGoal("retrieve the artifact", {0, 0, 0});
    npc.perceive(Percept{"saw", "thief", 1.0f, {1, 0, 0}}); // recorded into memory

    QuestContext ctx;
    ctx.npcName = npc.name();
    ctx.goal = npc.goal();
    ctx.memory = &npc.memory();
    ctx.world = sampleWorld();

    const Quest q = generateQuest(ctx);
    CHECK(q.valid);
    CHECK(q.giver == "Elara");
    CHECK(q.targetEntity == "artifact");

    const Dialogue d = generateDialogue(ctx, q);
    CHECK(anyContains(d.lines, "thief")); // consistent with what the NPC remembers
}

static void testLlmPathAndOfflineFallback() {
    Memory mem;
    const QuestContext ctx = sampleContext(mem);

    // A grounded LLM reply is used (note it picks a different real location).
    CompletionFn grounded = [](const std::string&) {
        return std::string("target=artifact; location=north gate; verb=recover;");
    };
    const Quest q = generateQuestLlm(ctx, grounded);
    CHECK(q.valid);
    CHECK(q.targetEntity == "artifact");
    CHECK(q.location == "north gate");

    // No provider attached: authorQuest falls back to the offline generator.
    const Quest off = authorQuest(ctx, CompletionFn{});
    CHECK(off.valid);
    CHECK(off.location == "market square"); // the rule-based choice
}

int main() {
    testQuestReferencesRealWorldState();
    testConsistentWithGoalAndMemory();
    testGroundingRejectsUngrounded();
    testBuiltFromNpc();
    testLlmPathAndOfflineFallback();

    if (g_failures == 0) {
        std::printf("All quest-generation tests passed.\n");
        return 0;
    }
    std::printf("%d quest-generation test(s) failed.\n", g_failures);
    return 1;
}
