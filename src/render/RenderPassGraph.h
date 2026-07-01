#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file RenderPassGraph.h
 * @brief Declarative render-pass dependency graph (issue #226, step 1).
 *
 * The first, isolated step of rendering modernization
 * (see `docs/RENDERING_MODERNIZATION.md`): describe the frame as named passes with
 * explicit dependencies (shadow -> gbuffer -> lighting -> post ...), then compute a
 * valid execution order instead of hard-coding the sequence in the renderer. This
 * makes reordering/inserting passes (deferred shading, HDR, extra post) a data
 * change rather than a code change.
 *
 * This header is intentionally standalone - it is not yet wired into the GL
 * renderer, so it cannot regress existing scenes - and it is header-only and
 * dependency-free, so the ordering logic is unit-testable without a GL context.
 * Ordering is deterministic: among passes whose dependencies are all satisfied,
 * the earliest-added one runs first.
 */
namespace IKore {
namespace render {

class RenderPassGraph {
public:
    /// Register a pass by name, with the names of passes that must run before it.
    /// Duplicate names are ignored (the first registration wins), so insertion
    /// order - and therefore the deterministic tie-break order - stays stable.
    void addPass(const std::string& name, std::vector<std::string> dependencies = {}) {
        if (m_index.count(name) != 0) return;
        m_index.emplace(name, m_passes.size());
        m_passes.push_back(Pass{name, std::move(dependencies)});
    }

    bool hasPass(const std::string& name) const { return m_index.count(name) != 0; }
    std::size_t passCount() const { return m_passes.size(); }

    /// True if every dependency listed by every pass names a registered pass.
    bool dependenciesResolved() const {
        for (const Pass& pass : m_passes) {
            for (const std::string& dep : pass.dependencies) {
                if (m_index.count(dep) == 0) return false;
            }
        }
        return true;
    }

    /**
     * @brief A topological execution order: every pass appears after all of its
     *        (registered) dependencies. Dependencies that name an unregistered
     *        pass impose no constraint. Returns an empty vector if the graph is
     *        empty or contains a dependency cycle (see hasCycle()).
     */
    std::vector<std::string> order() const {
        std::vector<bool> done(m_passes.size(), false);
        std::vector<std::string> result;
        result.reserve(m_passes.size());
        std::size_t remaining = m_passes.size();

        while (remaining > 0) {
            bool progressed = false;
            for (std::size_t i = 0; i < m_passes.size(); ++i) {
                if (done[i] || !ready(i, done)) continue;
                done[i] = true;
                result.push_back(m_passes[i].name);
                --remaining;
                progressed = true;
            }
            if (!progressed) return {}; // remaining passes form a cycle
        }
        return result;
    }

    /// True if the graph contains a dependency cycle.
    bool hasCycle() const { return !m_passes.empty() && order().empty(); }

    void clear() {
        m_passes.clear();
        m_index.clear();
    }

private:
    struct Pass {
        std::string name;
        std::vector<std::string> dependencies;
    };

    // A pass is ready when all its registered dependencies are already done.
    bool ready(std::size_t i, const std::vector<bool>& done) const {
        for (const std::string& dep : m_passes[i].dependencies) {
            auto it = m_index.find(dep);
            if (it == m_index.end()) continue; // unregistered dep: not a constraint
            if (!done[it->second]) return false;
        }
        return true;
    }

    std::vector<Pass> m_passes;
    std::unordered_map<std::string, std::size_t> m_index;
};

} // namespace render
} // namespace IKore
