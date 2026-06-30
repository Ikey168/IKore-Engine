#pragma once

#include "ai/Brain.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

/**
 * @file LlmBrain.h
 * @brief Provider-agnostic LLM brain + offline fallback composer (issue #156).
 *
 * Milestone 13. LlmBrain drives NPC decisions through an injected text-completion
 * function (CompletionFn). The engine therefore links no specific AI provider:
 * the caller supplies any backend (a hosted API, a local model, or a mock in
 * tests). LlmBrain formats the BrainContext + memory into a prompt, calls the
 * completion function, and parses the reply into a Decision.
 *
 * If no completion function is set (offline) or the reply cannot be parsed,
 * LlmBrain returns a Decision with valid=false. FallbackBrain uses that to defer
 * to a second brain (typically a BehaviorTreeBrain), so the engine has no hard
 * dependency on any single AI provider and keeps working offline.
 */
namespace IKore {
namespace ai {

/// Provider-agnostic text completion: prompt in, model reply out.
using CompletionFn = std::function<std::string(const std::string& prompt)>;

class LlmBrain : public IBrain {
public:
    explicit LlmBrain(CompletionFn complete, std::string systemPrompt = defaultSystemPrompt())
        : m_complete(std::move(complete)), m_system(std::move(systemPrompt)) {}

    /// True if a completion backend is attached (an LLM provider is available).
    bool usable() const { return static_cast<bool>(m_complete); }

    Decision think(const BrainContext& ctx) override {
        Decision decision;
        if (!m_complete) { // offline: no provider attached
            decision.valid = false;
            decision.rationale = "no LLM provider available";
            return decision;
        }
        const std::string reply = m_complete(buildPrompt(ctx));
        if (!parseDecision(reply, decision)) {
            decision = Decision{};
            decision.valid = false; // unparseable reply: let the caller fall back
            decision.rationale = "unparseable LLM reply";
        }
        return decision;
    }

    const char* name() const override { return "LlmBrain"; }

    /// Build the prompt from the NPC's goal, position, percepts, and memory.
    std::string buildPrompt(const BrainContext& ctx) const {
        std::string p = m_system;
        p += "\nNPC: " + ctx.npcName;
        p += "\nGoal: " + (ctx.goal.empty() ? std::string("(none)") : ctx.goal);
        p += "\nAt goal: " + std::string(ctx.atGoal() ? "yes" : "no");
        p += "\nPerceived now:";
        if (ctx.percepts.empty()) {
            p += " (nothing)";
        } else {
            for (const Percept& pc : ctx.percepts) p += " [" + pc.type + ":" + pc.subject + "]";
        }
        p += "\nMemory:";
        if (ctx.memory == nullptr || ctx.memory->size() == 0) {
            p += " (empty)";
        } else {
            for (const std::string& n : ctx.memory->notes()) p += " - " + n;
        }
        p += "\nReply with one JSON object: {\"action\": \"...\", \"target\": \"...\", "
             "\"rationale\": \"...\"}.";
        return p;
    }

    /**
     * @brief Parse a model reply into a Decision. Tolerant of prose around the
     *        JSON (reads the first quoted value after each key). "action" is
     *        required; "target"/"rationale" are optional.
     * @return false if no "action" field is found.
     */
    static bool parseDecision(const std::string& reply, Decision& out) {
        std::string action;
        if (!extractField(reply, "action", action) || action.empty()) return false;
        out.action = action;
        extractField(reply, "target", out.target);
        extractField(reply, "rationale", out.rationale);
        out.valid = true;
        return true;
    }

    static std::string defaultSystemPrompt() {
        return "You are an NPC decision-maker. Choose one action that pursues the goal "
               "and reacts to what is perceived.";
    }

private:
    /// Find `"key"`, then the next `"..."` value after the following colon.
    static bool extractField(const std::string& s, const std::string& key, std::string& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t k = s.find(needle);
        if (k == std::string::npos) return false;
        std::size_t colon = s.find(':', k + needle.size());
        if (colon == std::string::npos) return false;
        std::size_t q1 = s.find('"', colon + 1);
        if (q1 == std::string::npos) return false;
        std::string value;
        for (std::size_t i = q1 + 1; i < s.size(); ++i) {
            const char c = s[i];
            if (c == '\\' && i + 1 < s.size()) { // unescape
                const char e = s[++i];
                switch (e) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    default: value += e; break;
                }
                continue;
            }
            if (c == '"') {
                out = value;
                return true;
            }
            value += c;
        }
        return false; // unterminated string
    }

    CompletionFn m_complete;
    std::string m_system;
};

/**
 * @brief Try a primary brain (e.g. LlmBrain); if it cannot decide (offline or an
 *        unparseable reply -> Decision.valid == false), defer to a fallback brain
 *        (e.g. BehaviorTreeBrain). This is what removes any hard dependency on a
 *        single AI provider.
 */
class FallbackBrain : public IBrain {
public:
    FallbackBrain(std::unique_ptr<IBrain> primary, std::unique_ptr<IBrain> fallback)
        : m_primary(std::move(primary)), m_fallback(std::move(fallback)) {}

    Decision think(const BrainContext& ctx) override {
        if (m_primary) {
            Decision d = m_primary->think(ctx);
            if (d.valid) {
                m_lastUsedPrimary = true;
                return d;
            }
        }
        m_lastUsedPrimary = false;
        return m_fallback->think(ctx);
    }

    const char* name() const override { return "FallbackBrain"; }

    /// True if the most recent think() was answered by the primary brain.
    bool lastUsedPrimary() const { return m_lastUsedPrimary; }

private:
    std::unique_ptr<IBrain> m_primary;
    std::unique_ptr<IBrain> m_fallback;
    bool m_lastUsedPrimary{false};
};

} // namespace ai
} // namespace IKore
