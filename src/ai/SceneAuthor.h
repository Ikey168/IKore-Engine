#pragma once

#include "ai/LlmBrain.h" // CompletionFn (provider-agnostic LLM)
#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"
#include "world/SvgFloorPlanImporter.h" // world::Box / FloorPlan / emitToRegistry

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file SceneAuthor.h
 * @brief Natural-language scene authoring (issue #157).
 *
 * Milestone 13. Turns a typed prompt - e.g. "add a market square with 8 stalls
 * and a fountain" - into coherent entity placements. Two responsibilities are
 * kept separate:
 *   1. Interpret the prompt into a structured SceneRequest (which objects, how
 *      many, and the layout). The default interpreter is a deterministic, offline
 *      rule-based parser, so authoring needs no AI provider and is reproducible;
 *      an optional LLM-backed interpreter (via the #156 CompletionFn) handles
 *      free-form prompts, with the rule-based parser as the offline fallback.
 *   2. Lay the request out as world::Box primitives and place them through the
 *      M11 importer/scene-graph path (world::FloorPlan + world::emitToRegistry),
 *      so generated content reuses the World-from-Data primitives rather than
 *      ad-hoc placement code.
 *
 * Header-only; the interpret/layout steps are pure and unit-tested in isolation.
 */
namespace IKore {
namespace ai {

/// A recognized object kind and how many to place.
struct ObjectSpec {
    std::string kind;
    int count{1};
};

/// Structured interpretation of a prompt.
struct SceneRequest {
    std::string layout{"cluster"};   ///< "square" (a plaza/market) or "cluster".
    std::vector<ObjectSpec> objects; ///< props to place (stall, fountain, tree, ...).
    bool understood{false};          ///< false if nothing was recognized.

    int countOf(const std::string& kind) const {
        for (const ObjectSpec& o : objects) {
            if (o.kind == kind) return o.count;
        }
        return 0;
    }
    bool has(const std::string& kind) const { return countOf(kind) > 0; }
};

/// A labeled placement: an object kind and the box that realizes it.
struct Placement {
    std::string kind;
    world::Box box;
};

/// The result of authoring: the request, labeled placements, the scene-graph
/// primitive that was emitted, and how many entities it created.
struct AuthoredScene {
    SceneRequest request;
    std::vector<Placement> placements;
    world::FloorPlan plan;
    std::size_t entitiesCreated{0};
};

namespace detail {

/// Footprint/height of each object kind (XZ extent, Y height).
inline ecs::Vec3 boxSizeFor(const std::string& kind) {
    if (kind == "stall") return {1.5f, 2.0f, 1.5f};
    if (kind == "fountain") return {2.0f, 1.0f, 2.0f};
    if (kind == "tree") return {1.0f, 4.0f, 1.0f};
    if (kind == "bench") return {1.5f, 0.5f, 0.6f};
    if (kind == "lamp") return {0.3f, 3.0f, 0.3f};
    if (kind == "statue") return {1.0f, 2.5f, 1.0f};
    if (kind == "tower") return {3.0f, 8.0f, 3.0f};
    if (kind == "house") return {4.0f, 3.0f, 4.0f};
    return {1.0f, 1.0f, 1.0f};
}

/// Lowercase alphanumeric tokens of @p text (digits kept as their own tokens).
inline std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            cur += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        } else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

/// Canonical object kind for a token, or "" if it is not an object word.
inline std::string canonicalKind(const std::string& token) {
    static const std::unordered_map<std::string, std::string> kinds = {
        {"stall", "stall"},     {"stalls", "stall"},   {"fountain", "fountain"},
        {"fountains", "fountain"}, {"tree", "tree"},   {"trees", "tree"},
        {"bench", "bench"},     {"benches", "bench"},  {"lamp", "lamp"},
        {"lamps", "lamp"},      {"lamppost", "lamp"},  {"lampposts", "lamp"},
        {"statue", "statue"},   {"statues", "statue"}, {"tower", "tower"},
        {"towers", "tower"},    {"house", "house"},    {"houses", "house"},
    };
    auto it = kinds.find(token);
    return it == kinds.end() ? std::string() : it->second;
}

/// True if a token names a plaza-style layout.
inline bool isSquareWord(const std::string& token) {
    return token == "square" || token == "plaza" || token == "market";
}

/// Parse a quantity token (a digit string or a number word); -1 if not a number.
inline int parseQuantity(const std::string& token) {
    static const std::unordered_map<std::string, int> words = {
        {"a", 1},   {"an", 1},   {"one", 1},   {"two", 2},   {"three", 3},
        {"four", 4},{"five", 5}, {"six", 6},   {"seven", 7}, {"eight", 8},
        {"nine", 9},{"ten", 10}, {"eleven", 11}, {"twelve", 12},
    };
    auto it = words.find(token);
    if (it != words.end()) return it->second;
    if (!token.empty() && std::isdigit(static_cast<unsigned char>(token[0]))) {
        return static_cast<int>(std::strtol(token.c_str(), nullptr, 10));
    }
    return -1;
}

} // namespace detail

/// Ring radius used to arrange @p stallCount stalls around the square center.
inline float stallRingRadius(int stallCount) {
    const float kPi = 3.14159265f;
    const float stallSpacing = 1.5f * 1.6f; // footprint plus a gap
    const float byCount = stallCount > 0 ? (stallCount * stallSpacing) / (2.0f * kPi) : 0.0f;
    return byCount > 4.0f ? byCount : 4.0f;
}

/**
 * @brief Interpret a prompt with the offline rule-based parser.
 *
 * Recognizes object words (with counts from a preceding number, default 1) and a
 * plaza/market/square layout. First mention of each kind wins.
 */
inline SceneRequest interpretPrompt(const std::string& prompt) {
    const std::vector<std::string> tokens = detail::tokenize(prompt);
    SceneRequest req;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (detail::isSquareWord(tokens[i])) req.layout = "square";
        const std::string kind = detail::canonicalKind(tokens[i]);
        if (kind.empty() || req.has(kind)) continue; // first mention of each kind wins
        int count = 1;
        if (i > 0) {
            const int q = detail::parseQuantity(tokens[i - 1]);
            if (q >= 0) count = q;
        }
        if (count > 0) req.objects.push_back(ObjectSpec{kind, count});
    }
    req.understood = req.layout == "square" || !req.objects.empty();
    return req;
}

/**
 * @brief Interpret a prompt via an injected LLM completion function.
 *
 * Asks the model for a compact "key=value;" list (layout=... plus kind=count
 * pairs) and parses it. Returns an unrecognized request (understood == false) if
 * no provider is attached or the reply yields nothing, so callers can fall back.
 */
inline SceneRequest interpretPromptLlm(const std::string& prompt, const CompletionFn& complete) {
    SceneRequest req;
    if (!complete) return req; // offline: nothing understood, caller falls back
    const std::string ask =
        "Convert the scene request into a compact list of 'key=value;' pairs. Use "
        "layout=square for a plaza or market. Other keys are object kinds (stall, "
        "fountain, tree, bench, lamp, statue, tower, house) with integer counts. "
        "Reply with only the pairs. Request: " +
        prompt;
    const std::string reply = complete(ask);

    std::size_t i = 0;
    while (i < reply.size()) {
        std::size_t semi = reply.find(';', i);
        if (semi == std::string::npos) semi = reply.size();
        const std::string pair = reply.substr(i, semi - i);
        i = semi + 1;
        const std::size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        // Trim spaces around key and value.
        auto trim = [](std::string s) {
            std::size_t a = s.find_first_not_of(" \t\r\n");
            std::size_t b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        };
        const std::string key = trim(pair.substr(0, eq));
        const std::string val = trim(pair.substr(eq + 1));
        if (key == "layout") {
            if (detail::isSquareWord(val)) req.layout = "square";
        } else {
            const std::string kind = detail::canonicalKind(key);
            if (!kind.empty() && !req.has(kind)) {
                const int c = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
                if (c > 0) req.objects.push_back(ObjectSpec{kind, c});
            }
        }
    }
    req.understood = req.layout == "square" || !req.objects.empty();
    return req;
}

/**
 * @brief Lay a request out as boxes: a ground slab for a square layout, a central
 *        fountain, stalls evenly around a ring facing the center, and any other
 *        props on an outer ring. Fills a world::FloorPlan ready to emit.
 */
inline AuthoredScene layoutScene(const SceneRequest& req, const ecs::Vec3& anchor = ecs::Vec3{}) {
    const float kPi = 3.14159265f;
    const float groundThickness = 0.1f;

    AuthoredScene scene;
    scene.request = req;

    const int stalls = req.countOf("stall");
    const float ringR = stallRingRadius(stalls);
    const float margin = 2.0f;
    const float halfExtent = ringR + margin;

    auto rest = [&](const std::string& kind, const ecs::Vec3& xz, float yaw) {
        const ecs::Vec3 size = detail::boxSizeFor(kind);
        world::Box b;
        b.center = ecs::Vec3{xz.x, size.y * 0.5f, xz.z}; // sit on the ground plane (y = 0)
        b.size = size;
        b.yaw = yaw;
        scene.placements.push_back(Placement{kind, b});
    };

    // Ground slab for a plaza/market.
    if (req.layout == "square") {
        world::Box ground;
        ground.center = ecs::Vec3{anchor.x, -groundThickness * 0.5f, anchor.z};
        ground.size = ecs::Vec3{2.0f * halfExtent, groundThickness, 2.0f * halfExtent};
        scene.placements.push_back(Placement{"square", ground});
    }

    // Fountain(s) at the center (clustered if more than one).
    const int fountains = req.countOf("fountain");
    for (int f = 0; f < fountains; ++f) {
        const float off = fountains > 1 ? 1.5f : 0.0f;
        const float a = fountains > 1 ? (2.0f * kPi * f) / fountains : 0.0f;
        const ecs::Vec3 xz{anchor.x + std::cos(a) * off, 0.0f, anchor.z + std::sin(a) * off};
        rest("fountain", xz, 0.0f);
    }

    // Stalls evenly around the ring, each facing the center.
    for (int s = 0; s < stalls; ++s) {
        const float a = (2.0f * kPi * s) / stalls;
        const ecs::Vec3 xz{anchor.x + std::cos(a) * ringR, 0.0f, anchor.z + std::sin(a) * ringR};
        rest("stall", xz, a + kPi);
    }

    // Remaining prop kinds spread around an outer ring.
    std::vector<std::string> others;
    for (const ObjectSpec& o : req.objects) {
        if (o.kind == "stall" || o.kind == "fountain") continue;
        for (int k = 0; k < o.count; ++k) others.push_back(o.kind);
    }
    for (std::size_t j = 0; j < others.size(); ++j) {
        const float a = others.size() > 0 ? (2.0f * kPi * j) / others.size() : 0.0f;
        const float r = halfExtent - 0.75f;
        const ecs::Vec3 xz{anchor.x + std::cos(a) * r, 0.0f, anchor.z + std::sin(a) * r};
        rest(others[j], xz, a + kPi);
    }

    // Realize as the importer/scene-graph primitive (FloorPlan of boxes).
    for (const Placement& p : scene.placements) {
        if (p.kind == "square") {
            scene.plan.floor = p.box;
            scene.plan.hasFloor = true;
        } else {
            scene.plan.walls.push_back(p.box);
        }
    }
    return scene;
}

/// Author from a prompt with the offline rule-based interpreter, placing entities
/// through world::emitToRegistry. Returns the authored scene (with entity count).
inline AuthoredScene authorScene(const std::string& prompt, ecs::Registry& registry,
                                 const ecs::Vec3& anchor = ecs::Vec3{}) {
    AuthoredScene scene = layoutScene(interpretPrompt(prompt), anchor);
    scene.entitiesCreated = world::emitToRegistry(scene.plan, registry);
    return scene;
}

/// Author using an injected LLM, falling back to the offline rule-based parser if
/// no provider is attached or the model's reply is not understood.
inline AuthoredScene authorScene(const std::string& prompt, ecs::Registry& registry,
                                 const CompletionFn& complete,
                                 const ecs::Vec3& anchor = ecs::Vec3{}) {
    SceneRequest req = interpretPromptLlm(prompt, complete);
    if (!req.understood) req = interpretPrompt(prompt); // offline fallback
    AuthoredScene scene = layoutScene(req, anchor);
    scene.entitiesCreated = world::emitToRegistry(scene.plan, registry);
    return scene;
}

} // namespace ai
} // namespace IKore
