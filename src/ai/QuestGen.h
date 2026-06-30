#pragma once

#include "ai/Brain.h"    // Memory
#include "ai/LlmBrain.h" // CompletionFn (provider-agnostic LLM)

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

/**
 * @file QuestGen.h
 * @brief Procedural quests & dialogue from world state (issue #158).
 *
 * Milestone 13. Generates a quest and NPC dialogue derived from the current world
 * state (locations, entities, recent events) and an NPC's goal + memory.
 *
 * Two properties are guaranteed by construction:
 *   - Grounded: the quest only references real world state. The rule-based
 *     generator picks its target/location/event from the supplied WorldState, and
 *     the optional LLM generator's output is validated against that WorldState
 *     (an entity or location it did not list is rejected), so neither can
 *     hallucinate content that does not exist.
 *   - Consistent with the NPC: the objective verb is derived from the NPC goal and
 *     the target is chosen to match the goal first, while the dialogue references
 *     a remembered subject that is itself grounded in world state.
 *
 * The rule-based generator needs no AI provider (offline, deterministic). The LLM
 * generator falls back to it, so there is no hard dependency on any provider.
 * Header-only; pure (std + Memory + CompletionFn).
 */
namespace IKore {
namespace ai {

/// A snapshot of the world the generator may reference.
struct WorldState {
    std::vector<std::string> locations; ///< e.g. "market square", "north gate".
    std::vector<std::string> entities;  ///< e.g. "artifact", "merchant", "thief".
    std::vector<std::string> events;    ///< recent events, e.g. "a theft at the market square".

    static bool contains(const std::vector<std::string>& v, const std::string& s) {
        for (const std::string& x : v) {
            if (x == s) return true;
        }
        return false;
    }
    bool hasLocation(const std::string& s) const { return contains(locations, s); }
    bool hasEntity(const std::string& s) const { return contains(entities, s); }
};

/// Inputs for generation: who is giving the quest, their goal, memory, and world.
struct QuestContext {
    std::string npcName;
    std::string goal;
    const Memory* memory{nullptr};
    WorldState world;
};

/// A generated quest grounded in world state.
struct Quest {
    std::string title;
    std::string giver;
    std::string verb;            ///< canonical action ("recover", "defend", ...).
    std::string targetEntity;    ///< a real WorldState entity.
    std::string location;        ///< a real WorldState location.
    std::string referencedEvent; ///< a real WorldState event (may be empty).
    std::string objective;       ///< one-line objective text.
    std::vector<std::string> steps;
    bool valid{false};
};

/// Generated NPC dialogue, consistent with the NPC's memory and goal.
struct Dialogue {
    std::string speaker;
    std::vector<std::string> lines;
    bool valid{false};
};

namespace detail {

inline std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline bool mentions(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return false;
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

inline std::string capitalize(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

/// Subject after the "type:subject" separator in a memory note.
inline std::string subjectOf(const std::string& note) {
    const std::size_t p = note.find(':');
    return p == std::string::npos ? note : note.substr(p + 1);
}

/// Canonical quest verb implied by an NPC goal.
inline std::string verbForGoal(const std::string& goal) {
    const std::string g = toLower(goal);
    auto any = [&](std::initializer_list<const char*> words) {
        for (const char* w : words) {
            if (g.find(w) != std::string::npos) return true;
        }
        return false;
    };
    if (any({"retriev", "recover", "find", "fetch", "collect", "get "})) return "recover";
    if (any({"defend", "guard", "protect"})) return "defend";
    if (any({"defeat", "hunt", "kill", "slay", "clear"})) return "defeat";
    if (any({"deliver", "escort", "bring"})) return "deliver";
    if (any({"reach", "escape", "exit", "flee"})) return "reach";
    return "investigate";
}

} // namespace detail

/**
 * @brief Generate a quest from world state + the NPC goal/memory (rule-based).
 *
 * Target selection prefers (1) an entity named in the goal, (2) a remembered
 * subject that is a world entity, (3) an entity named in an event, (4) the first
 * entity. The location prefers one named by an event, else the first. Returns an
 * invalid quest if the world has no entities/locations to ground it.
 */
inline Quest generateQuest(const QuestContext& ctx) {
    Quest q;
    q.giver = ctx.npcName;
    const WorldState& w = ctx.world;
    if (w.entities.empty() || w.locations.empty()) return q; // cannot ground

    // Target.
    std::string target;
    for (const std::string& e : w.entities) {
        if (detail::mentions(ctx.goal, e)) { target = e; break; } // goal-aligned
    }
    if (target.empty() && ctx.memory != nullptr) {
        for (const std::string& note : ctx.memory->notes()) {
            const std::string subj = detail::subjectOf(note);
            if (w.hasEntity(subj)) { target = subj; break; } // remembered + real
        }
    }
    if (target.empty()) {
        for (const std::string& ev : w.events) {
            for (const std::string& e : w.entities) {
                if (detail::mentions(ev, e)) { target = e; break; }
            }
            if (!target.empty()) break;
        }
    }
    if (target.empty()) target = w.entities.front();

    // Location: prefer one named by an event.
    std::string location;
    for (const std::string& ev : w.events) {
        for (const std::string& loc : w.locations) {
            if (detail::mentions(ev, loc)) { location = loc; break; }
        }
        if (!location.empty()) break;
    }
    if (location.empty()) location = w.locations.front();

    // Referenced event: prefer one mentioning the target or location.
    std::string event;
    for (const std::string& ev : w.events) {
        if (detail::mentions(ev, target) || detail::mentions(ev, location)) { event = ev; break; }
    }
    if (event.empty() && !w.events.empty()) event = w.events.front();

    q.verb = detail::verbForGoal(ctx.goal);
    q.targetEntity = target;
    q.location = location;
    q.referencedEvent = event;
    q.title = detail::capitalize(q.verb) + " the " + target;
    q.objective = detail::capitalize(q.verb) + " the " + target + " at the " + location + ".";
    if (!event.empty()) q.steps.push_back("Look into " + event + ".");
    q.steps.push_back("Travel to the " + location + ".");
    q.steps.push_back(detail::capitalize(q.verb) + " the " + target + ".");
    q.valid = true;
    return q;
}

/// Generate NPC dialogue for @p quest, consistent with the NPC's memory and goal.
inline Dialogue generateDialogue(const QuestContext& ctx, const Quest& quest) {
    Dialogue d;
    d.speaker = ctx.npcName;
    if (!quest.valid) return d;

    d.lines.push_back("I am " + ctx.npcName + ", and I could use your help.");
    // Objective line: references the goal-derived target and a real location.
    d.lines.push_back("Please " + quest.verb + " the " + quest.targetEntity + " at the " +
                      quest.location + ".");
    // Memory line: reference a remembered subject that is grounded in world state.
    if (ctx.memory != nullptr) {
        for (const std::string& note : ctx.memory->notes()) {
            const std::string subj = detail::subjectOf(note);
            bool grounded = ctx.world.hasEntity(subj);
            if (!grounded) {
                for (const std::string& ev : ctx.world.events) {
                    if (detail::mentions(ev, subj)) { grounded = true; break; }
                }
            }
            if (grounded) {
                d.lines.push_back("I have not forgotten the " + subj + ".");
                break;
            }
        }
    }
    if (!quest.referencedEvent.empty()) {
        d.lines.push_back("Ever since " + quest.referencedEvent + ", we have needed aid.");
    }
    d.valid = true;
    return d;
}

/**
 * @brief Generate a quest via an injected LLM, validated against world state.
 *
 * Asks for "key=value;" pairs (target/location/verb). Any target/location the
 * model returns that is not in the WorldState is rejected (the quest comes back
 * invalid), so LLM output cannot reference world state that does not exist.
 * Returns invalid if no provider is attached, so callers can fall back.
 */
inline Quest generateQuestLlm(const QuestContext& ctx, const CompletionFn& complete) {
    Quest q;
    q.giver = ctx.npcName;
    if (!complete) return q;

    std::string prompt = "Write a quest grounded ONLY in this world. Reply with "
                         "'key=value;' pairs: target, location, verb.\nLocations:";
    for (const std::string& l : ctx.world.locations) prompt += " [" + l + "]";
    prompt += "\nEntities:";
    for (const std::string& e : ctx.world.entities) prompt += " [" + e + "]";
    prompt += "\nEvents:";
    for (const std::string& ev : ctx.world.events) prompt += " [" + ev + "]";
    prompt += "\nNPC: " + ctx.npcName + "\nGoal: " + ctx.goal;
    const std::string reply = complete(prompt);

    std::string target, location, verb;
    std::size_t i = 0;
    while (i < reply.size()) {
        std::size_t semi = reply.find(';', i);
        if (semi == std::string::npos) semi = reply.size();
        const std::string pair = reply.substr(i, semi - i);
        i = semi + 1;
        const std::size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            const std::size_t a = s.find_first_not_of(" \t\r\n");
            const std::size_t b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        };
        const std::string key = trim(pair.substr(0, eq));
        const std::string val = trim(pair.substr(eq + 1));
        if (key == "target") target = val;
        else if (key == "location") location = val;
        else if (key == "verb") verb = val;
    }

    // Grounding check: reject anything not in the world state.
    if (!ctx.world.hasEntity(target) || !ctx.world.hasLocation(location)) return q;

    std::string event;
    for (const std::string& ev : ctx.world.events) {
        if (detail::mentions(ev, target) || detail::mentions(ev, location)) { event = ev; break; }
    }
    if (event.empty() && !ctx.world.events.empty()) event = ctx.world.events.front();

    q.verb = verb.empty() ? detail::verbForGoal(ctx.goal) : detail::toLower(verb);
    q.targetEntity = target;
    q.location = location;
    q.referencedEvent = event;
    q.title = detail::capitalize(q.verb) + " the " + target;
    q.objective = detail::capitalize(q.verb) + " the " + target + " at the " + location + ".";
    if (!event.empty()) q.steps.push_back("Look into " + event + ".");
    q.steps.push_back("Travel to the " + location + ".");
    q.steps.push_back(detail::capitalize(q.verb) + " the " + target + ".");
    q.valid = true;
    return q;
}

/// Generate a quest via the LLM, falling back to the rule-based generator when no
/// provider is attached or the model's reply is not grounded in world state.
inline Quest authorQuest(const QuestContext& ctx, const CompletionFn& complete) {
    Quest q = generateQuestLlm(ctx, complete);
    if (!q.valid) q = generateQuest(ctx);
    return q;
}

} // namespace ai
} // namespace IKore
