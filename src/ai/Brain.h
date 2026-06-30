#pragma once

#include "core/ecs/components/Components.h"

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

/**
 * @file Brain.h
 * @brief Swappable NPC decision-making interface (issue #156).
 *
 * Milestone 13 (AI-Native Authoring & NPCs) foundation. Defines a provider-
 * agnostic brain abstraction: an NPC hands a brain its situation (a BrainContext
 * - goal, position, what it just perceived, and its memory) and the brain returns
 * a Decision (an action to take). Concrete brains are swappable behind IBrain:
 *   - LlmBrain (LlmBrain.h) drives decisions through an injected text-completion
 *     function, so the engine links no specific AI provider, and
 *   - BehaviorTreeBrain (BehaviorTree.h) decides with a plain behavior tree and
 *     no external dependency - the offline fallback.
 * FallbackBrain composes the two so the engine has no hard dependency on any
 * single AI provider.
 *
 * This header is pure (std + the header-only Vec3), so brains are unit-tested in
 * isolation; EventSystem wiring lives in Npc.h.
 */
namespace IKore {
namespace ai {

/// One thing an NPC perceives this tick (built from a world/EventSystem event).
struct Percept {
    std::string type;       ///< e.g. "threat", "item", "sound", "ally".
    std::string subject;    ///< who/what (an entity name or tag).
    float intensity{1.0f};  ///< strength/priority of the stimulus.
    ecs::Vec3 position{};    ///< where it happened, in world space.
};

/// A small bounded memory of recent observations (newest last).
class Memory {
public:
    explicit Memory(std::size_t capacity = 16) : m_capacity(capacity == 0 ? 1 : capacity) {}

    /// Record a human-readable note (also fed to an LLM prompt). Evicts oldest.
    void remember(const std::string& note) {
        m_notes.push_back(note);
        while (m_notes.size() > m_capacity) m_notes.pop_front();
    }

    /// True if any remembered note contains @p substr.
    bool recalls(const std::string& substr) const {
        for (const std::string& n : m_notes) {
            if (n.find(substr) != std::string::npos) return true;
        }
        return false;
    }

    std::size_t size() const { return m_notes.size(); }
    std::size_t capacity() const { return m_capacity; }
    void clear() { m_notes.clear(); }

    const std::deque<std::string>& notes() const { return m_notes; }

private:
    std::deque<std::string> m_notes;
    std::size_t m_capacity;
};

/// The NPC's situation, handed to a brain to decide on.
struct BrainContext {
    std::string npcName;
    std::string goal;               ///< current goal tag, e.g. "reach_exit".
    ecs::Vec3 selfPosition{};
    ecs::Vec3 goalPosition{};
    float arriveRadius{0.5f};       ///< treated as "at the goal" within this.
    std::vector<Percept> percepts;  ///< perceived since the last decision.
    const Memory* memory{nullptr};  ///< the NPC's memory (may be null).

    /// First percept of @p type this tick, or nullptr if none.
    const Percept* perceived(const std::string& type) const {
        for (const Percept& p : percepts) {
            if (p.type == type) return &p;
        }
        return nullptr;
    }

    /// True when the NPC is within arriveRadius of the goal position.
    bool atGoal() const { return (goalPosition - selfPosition).length() <= arriveRadius; }
};

/// A decision: an action plus optional target/destination and a rationale.
struct Decision {
    std::string action;     ///< e.g. "move_to", "flee", "interact", "idle".
    std::string target;     ///< optional subject of the action.
    ecs::Vec3 position{};    ///< optional destination in world space.
    std::string rationale;  ///< why - useful for LLM output and debugging.
    bool valid{true};       ///< false if the brain could not decide (see FallbackBrain).
};

/// Swappable NPC decision-maker.
class IBrain {
public:
    virtual ~IBrain() = default;

    /// Decide what to do given @p ctx.
    virtual Decision think(const BrainContext& ctx) = 0;

    /// Stable identifier for logging/debugging (e.g. "LlmBrain").
    virtual const char* name() const = 0;
};

} // namespace ai
} // namespace IKore
