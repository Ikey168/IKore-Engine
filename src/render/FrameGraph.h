#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "render/RenderPassGraph.h"

/**
 * @file FrameGraph.h
 * @brief The engine's concrete frame described as a RenderPassGraph, plus a small
 *        executor that runs named passes in the computed order (issue #232).
 *
 * `RenderPassGraph` (issue #226) only knows how to order names. This header is the
 * "adoption" step from `docs/RENDERING_MODERNIZATION.md` P1: it defines the real
 * pass set of the forward renderer as data (`buildDefaultFrameGraph()`) and a
 * `FrameGraphExecutor` that maps each pass name to a callback and invokes them in
 * dependency order. The renderer's frame loop drives `execute()` instead of a
 * hard-coded sequence, so reordering or inserting a pass becomes a data change
 * here rather than an edit to the render loop.
 *
 * Both pieces are GL-free and header-only, so the frame topology and the executor
 * are unit-testable without a graphics context (see tests/test_frame_graph.cpp).
 */
namespace IKore {
namespace render {

/// Canonical names of the passes that make up the default forward frame.
namespace passes {
inline constexpr const char* Shadow = "shadow";           ///< Shadow-map depth pass.
inline constexpr const char* SceneBegin = "scene_begin";  ///< Bind + clear the offscreen color target.
inline constexpr const char* Skybox = "skybox";           ///< Background skybox.
inline constexpr const char* Forward = "forward";         ///< Lit forward/opaque scene.
inline constexpr const char* Particles = "particles";     ///< Particle systems over the lit scene.
inline constexpr const char* PostProcess = "postprocess"; ///< Effect chain (bloom/SSAO/FXAA) + present.
} // namespace passes

/**
 * @brief Build the forward renderer's frame as a dependency graph.
 *
 * The dependencies encode the renderer's real data flow:
 *  - the forward lighting pass samples the shadow map, so it depends on @c shadow;
 *  - every pass that draws into the offscreen buffer depends on @c scene_begin,
 *    which binds and clears it;
 *  - the skybox is the background, so @c forward draws over it;
 *  - @c particles draw over the lit scene; and
 *  - @c postprocess consumes the finished color buffer.
 *
 * With the graph's deterministic tie-break (earliest-added ready pass first) this
 * yields exactly the order the renderer used before adoption:
 *   shadow -> scene_begin -> skybox -> forward -> particles -> postprocess
 * so wiring the graph in cannot reorder work and cannot regress existing scenes.
 */
inline RenderPassGraph buildDefaultFrameGraph() {
    RenderPassGraph graph;
    graph.addPass(passes::Shadow);
    graph.addPass(passes::SceneBegin);
    graph.addPass(passes::Skybox, {passes::SceneBegin});
    graph.addPass(passes::Forward, {passes::Shadow, passes::SceneBegin, passes::Skybox});
    graph.addPass(passes::Particles, {passes::Forward});
    graph.addPass(passes::PostProcess, {passes::Particles});
    return graph;
}

/**
 * @brief Runs named render passes in the order computed by a RenderPassGraph.
 *
 * The graph decides @e when each pass runs (topological order); the handler
 * registered for a pass decides @e what it does. Reordering or inserting a pass is
 * therefore a change to the graph, not to the render loop. A pass with no
 * registered handler is a no-op, so passes can be wired up incrementally.
 *
 * The execution order is computed once at construction (the graph is static), so
 * per-frame `execute()` only walks the precomputed order.
 */
class FrameGraphExecutor {
public:
    using Handler = std::function<void()>;

    explicit FrameGraphExecutor(RenderPassGraph graph)
        : m_graph(std::move(graph)), m_order(m_graph.order()) {}

    /// Register (or replace) the handler invoked for @p pass.
    void setHandler(const std::string& pass, Handler handler) {
        m_handlers[pass] = std::move(handler);
    }

    /// Invoke each pass's handler in dependency order; passes without a handler
    /// are skipped.
    void execute() const {
        for (const std::string& pass : m_order) {
            auto it = m_handlers.find(pass);
            if (it != m_handlers.end() && it->second) it->second();
        }
    }

    const RenderPassGraph& graph() const { return m_graph; }

    /// The precomputed execution order (empty if the graph contained a cycle).
    const std::vector<std::string>& order() const { return m_order; }

private:
    RenderPassGraph m_graph;
    std::vector<std::string> m_order;
    std::unordered_map<std::string, Handler> m_handlers;
};

} // namespace render
} // namespace IKore
