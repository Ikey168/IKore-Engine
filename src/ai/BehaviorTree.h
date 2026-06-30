#pragma once

#include "ai/Brain.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @file BehaviorTree.h
 * @brief A minimal behavior tree and the offline fallback brain (issue #156).
 *
 * Milestone 13. A small, dependency-free behavior tree (Sequence / Selector /
 * Condition / Action / Inverter) plus BehaviorTreeBrain, an IBrain that runs a
 * tree to decide. Because it needs nothing external, it is the offline fallback
 * when no AI provider is available, so the engine has no hard dependency on any
 * single provider.
 *
 * makeDefaultNpcBrain() builds a tree that reacts to perceived threats (flee),
 * otherwise pursues the goal (move toward it), otherwise idles - enough for an
 * NPC to "pursue goals and react to world events" with zero dependencies.
 */
namespace IKore {
namespace ai {

enum class Status { Success, Failure, Running };

/// Working state a tree node sees: the read-only context and the Decision it fills.
struct BtState {
    const BrainContext& ctx;
    Decision& out;
};

/// A behavior-tree node: inspects state, may write @c out, returns a status.
using BtNode = std::function<Status(BtState&)>;

/// Run children in order; fail at the first failure, else succeed (AND).
inline BtNode sequence(std::vector<BtNode> children) {
    return [children = std::move(children)](BtState& s) {
        for (const BtNode& child : children) {
            const Status st = child(s);
            if (st != Status::Success) return st;
        }
        return Status::Success;
    };
}

/// Run children in order; succeed at the first success, else fail (OR).
inline BtNode selector(std::vector<BtNode> children) {
    return [children = std::move(children)](BtState& s) {
        for (const BtNode& child : children) {
            const Status st = child(s);
            if (st != Status::Failure) return st;
        }
        return Status::Failure;
    };
}

/// Succeed iff the predicate holds (a guard that writes nothing).
inline BtNode condition(std::function<bool(const BrainContext&)> pred) {
    return [pred = std::move(pred)](BtState& s) {
        return pred(s.ctx) ? Status::Success : Status::Failure;
    };
}

/// Leaf that fills @c out via @p effect and succeeds.
inline BtNode action(std::function<void(const BrainContext&, Decision&)> effect) {
    return [effect = std::move(effect)](BtState& s) {
        effect(s.ctx, s.out);
        return Status::Success;
    };
}

/// Invert Success<->Failure (Running passes through).
inline BtNode inverter(BtNode child) {
    return [child = std::move(child)](BtState& s) {
        const Status st = child(s);
        if (st == Status::Success) return Status::Failure;
        if (st == Status::Failure) return Status::Success;
        return st;
    };
}

/// An IBrain that decides by running a behavior tree. Needs no AI provider.
class BehaviorTreeBrain : public IBrain {
public:
    explicit BehaviorTreeBrain(BtNode root, Decision fallback = makeIdle())
        : m_root(std::move(root)), m_fallback(std::move(fallback)) {}

    Decision think(const BrainContext& ctx) override {
        Decision out = m_fallback;
        BtState state{ctx, out};
        m_root(state);
        out.valid = true; // a tree always yields a usable decision (idle at worst)
        return out;
    }

    const char* name() const override { return "BehaviorTreeBrain"; }

    static Decision makeIdle() {
        Decision d;
        d.action = "idle";
        d.rationale = "nothing to do";
        return d;
    }

private:
    BtNode m_root;
    Decision m_fallback;
};

/**
 * @brief A sensible default NPC: flee perceived threats, else pursue the goal,
 *        else idle. Demonstrates pursuing goals and reacting to events offline.
 */
inline std::unique_ptr<IBrain> makeDefaultNpcBrain() {
    BtNode reactToThreat = sequence({
        condition([](const BrainContext& c) { return c.perceived("threat") != nullptr; }),
        action([](const BrainContext& c, Decision& d) {
            const Percept* threat = c.perceived("threat");
            d.action = "flee";
            d.target = threat->subject;
            // Head directly away from the threat.
            const ecs::Vec3 away = (c.selfPosition - threat->position).normalized();
            d.position = c.selfPosition + away * 5.0f;
            d.rationale = "fleeing " + threat->subject;
        }),
    });

    BtNode pursueGoal = sequence({
        condition([](const BrainContext& c) { return !c.goal.empty() && !c.atGoal(); }),
        action([](const BrainContext& c, Decision& d) {
            d.action = "move_to";
            d.target = c.goal;
            d.position = c.goalPosition;
            d.rationale = "pursuing goal " + c.goal;
        }),
    });

    BtNode idle = action([](const BrainContext&, Decision& d) {
        d.action = "idle";
        d.rationale = "at goal / nothing to react to";
    });

    return std::unique_ptr<IBrain>(
        new BehaviorTreeBrain(selector({std::move(reactToThreat), std::move(pursueGoal), std::move(idle)})));
}

} // namespace ai
} // namespace IKore
