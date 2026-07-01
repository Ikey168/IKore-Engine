#pragma once

#include "core/DebugConsole.h"
#include "core/PerfStats.h"

struct GLFWwindow;

/**
 * @file DebugUI.h
 * @brief Dear ImGui integration: the foundation for in-engine editor/debug UI.
 *
 * Milestone 9 (issue #144). Wraps ImGui setup, per-frame rendering, and teardown
 * behind a small interface so the engine's main loop only needs a few lines.
 * Docking is enabled (ImGuiConfigFlags_DockingEnable) so the panels added by the
 * follow-on issues (#53-#63: FPS overlay, console, entity inspector, scene
 * hierarchy, input remapping, HUD) can be docked into a layout.
 *
 * The overlay is toggleable and starts hidden; when hidden, render() returns
 * immediately so there is no per-frame ImGui cost.
 */
namespace IKore {

class DebugUI {
public:
    /**
     * @brief Initialize ImGui and its GLFW + OpenGL3 backends.
     * @param window       The GLFW window owning the current GL context.
     * @param glslVersion  GLSL #version string for the GL3 backend (e.g. "#version 330").
     * @return true on success.
     *
     * Call after the GL context is current and the loader is initialized.
     */
    bool initialize(GLFWwindow* window, const char* glslVersion);

    /// Tear down the ImGui backends and context. Safe to call if not initialized.
    void shutdown();

    /**
     * @brief Build and draw the debug UI for this frame.
     * @param deltaTimeSeconds Frame delta, shown in the stats panel.
     *
     * No-op (returns immediately) when the overlay is hidden or uninitialized,
     * so a hidden overlay costs nothing.
     */
    void render(float deltaTimeSeconds);

    /**
     * @brief Record this frame's timing into the perf stats (issue #53).
     *
     * Called every frame regardless of visibility - a cheap deque push - so the
     * FPS/frame-time graph has history the moment the overlay is opened, while a
     * hidden overlay still costs nothing to draw.
     */
    void update(float deltaTimeSeconds);

    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    void toggle() { m_visible = !m_visible; }

    /// Access the debug console so engine systems can register commands.
    DebugConsole& console() { return m_console; }

    /// Handle an ImGui InputText callback for the console (history/completion).
    /// Takes a void* (an ImGuiInputTextCallbackData*) to keep this header free of
    /// the ImGui headers; the implementation casts it. Public so the file-static
    /// callback trampoline in DebugUI.cpp can dispatch to it.
    int onConsoleTextEdit(void* callbackData);

private:
    void buildUI(float deltaTimeSeconds);
    void renderPerfOverlay();
    void renderConsole();

    bool m_initialized{false};
    bool m_visible{false};
    bool m_showDemoWindow{false};
    bool m_showPerf{true};
    bool m_showConsole{true};
    PerfStats m_perf;
    DebugConsole m_console;
    char m_consoleInput[256]{};
};

} // namespace IKore
