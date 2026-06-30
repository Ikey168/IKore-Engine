#pragma once

#include "ai/Brain.h"
#include "core/EventSystem.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @file Npc.h
 * @brief An NPC (memory + swappable brain) wired to the EventSystem (issue #156).
 *
 * Milestone 13. Npc owns a goal, a Memory, and an IBrain that can be swapped at
 * runtime. It consumes perceived events (recording them to memory) and, on
 * decide(), runs its brain over the current situation to choose an action.
 *
 * NpcEventBridge connects an Npc to the engine's EventSystem: it subscribes the
 * NPC to event types (consume - each event becomes a Percept) and publishes the
 * chosen Decision back as an event (emit), so NPCs "emit/consume EventSystem
 * events" without the brain layer depending on the event system.
 */
namespace IKore {
namespace ai {

class Npc {
public:
    Npc(std::string name, std::unique_ptr<IBrain> brain, std::size_t memoryCapacity = 16)
        : m_name(std::move(name)), m_brain(std::move(brain)), m_memory(memoryCapacity) {}

    void setGoal(std::string goal, const ecs::Vec3& goalPosition) {
        m_goal = std::move(goal);
        m_goalPosition = goalPosition;
    }
    void setPosition(const ecs::Vec3& position) { m_selfPosition = position; }
    void setArriveRadius(float r) { m_arriveRadius = r; }

    /// Swap the decision-making brain at runtime (LLM <-> behavior tree, etc.).
    void setBrain(std::unique_ptr<IBrain> brain) { m_brain = std::move(brain); }
    IBrain* brain() { return m_brain.get(); }

    /// Consume a perceived event: buffer it for the next decision and remember it.
    void perceive(const Percept& p) {
        m_percepts.push_back(p);
        m_memory.remember(p.type + ":" + p.subject);
    }

    /// Run the brain over the current context, then clear this tick's percepts.
    Decision decide() {
        BrainContext ctx;
        ctx.npcName = m_name;
        ctx.goal = m_goal;
        ctx.selfPosition = m_selfPosition;
        ctx.goalPosition = m_goalPosition;
        ctx.arriveRadius = m_arriveRadius;
        ctx.percepts = m_percepts;
        ctx.memory = &m_memory;

        Decision d = m_brain ? m_brain->think(ctx) : Decision{};
        m_percepts.clear();
        return d;
    }

    const std::string& name() const { return m_name; }
    const std::string& goal() const { return m_goal; }
    Memory& memory() { return m_memory; }
    const Memory& memory() const { return m_memory; }
    const ecs::Vec3& position() const { return m_selfPosition; }
    std::size_t pendingPercepts() const { return m_percepts.size(); }

private:
    std::string m_name;
    std::unique_ptr<IBrain> m_brain;
    Memory m_memory;
    std::string m_goal;
    ecs::Vec3 m_selfPosition{};
    ecs::Vec3 m_goalPosition{};
    float m_arriveRadius{0.5f};
    std::vector<Percept> m_percepts;
};

/// Connects an Npc to the EventSystem for consuming and emitting events.
class NpcEventBridge {
public:
    explicit NpcEventBridge(Npc& npc, EventSystem& events = EventSystem::getInstance(),
                            std::string actionEventType = "npc.action")
        : m_npc(npc), m_events(events), m_actionEventType(std::move(actionEventType)) {}

    ~NpcEventBridge() {
        for (const EventSubscription& sub : m_subs) m_events.unsubscribe(sub);
    }

    NpcEventBridge(const NpcEventBridge&) = delete;
    NpcEventBridge& operator=(const NpcEventBridge&) = delete;

    /**
     * @brief Subscribe to @p eventType; each event is consumed as a Percept.
     *
     * A TypedEventData<Percept> payload is used directly (its type filled in from
     * @p eventType if empty); any other payload becomes a minimal Percept tagged
     * with the event type, so generic world events are still perceived.
     */
    void listen(const std::string& eventType) {
        EventSubscription sub = m_events.subscribe(
            eventType, [this, eventType](const std::shared_ptr<EventData>& data) {
                Percept p;
                if (auto* typed = dynamic_cast<TypedEventData<Percept>*>(data.get())) {
                    p = typed->data;
                    if (p.type.empty()) p.type = eventType;
                } else {
                    p.type = eventType;
                }
                m_npc.perceive(p);
            });
        m_subs.push_back(sub);
    }

    /// Decide and publish the resulting action as an EventSystem event.
    Decision step() {
        Decision d = m_npc.decide();
        m_events.publish<Decision>(m_actionEventType, d);
        return d;
    }

private:
    Npc& m_npc;
    EventSystem& m_events;
    std::string m_actionEventType;
    std::vector<EventSubscription> m_subs;
};

} // namespace ai
} // namespace IKore
