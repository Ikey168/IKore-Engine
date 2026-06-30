#include "ui/DebugUI.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/Logger.h"

namespace IKore {

bool DebugUI::initialize(GLFWwindow* window, const char* glslVersion) {
    if (m_initialized) return true;
    if (window == nullptr) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // allow panels to be docked together

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true)) {
        LOG_ERROR("DebugUI: ImGui GLFW backend init failed");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
        LOG_ERROR("DebugUI: ImGui OpenGL3 backend init failed");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    LOG_INFO("DebugUI: Dear ImGui initialized (press F1 to toggle)");
    return true;
}

void DebugUI::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void DebugUI::render(float deltaTimeSeconds) {
    // Hidden or uninitialized overlay costs nothing.
    if (!m_initialized || !m_visible) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    buildUI(deltaTimeSeconds);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::buildUI(float deltaTimeSeconds) {
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("IKore Debug");
    ImGui::TextUnformatted("IKore Engine - in-engine debug UI");
    ImGui::Separator();

    ImGui::Text("FPS: %.1f  (%.3f ms/frame)", io.Framerate,
                io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Frame delta: %.3f ms", deltaTimeSeconds * 1000.0f);

    ImGui::Separator();
    ImGui::TextUnformatted("Planned panels (Milestone 9):");
    ImGui::BulletText("FPS / performance overlay (#53)");
    ImGui::BulletText("Debug console (#54)");
    ImGui::BulletText("HUD / menus (#55, #58)");
    ImGui::BulletText("Entity inspector (#56)");
    ImGui::BulletText("Scene hierarchy viewer (#59)");
    ImGui::BulletText("Input remapping (#60)");
    ImGui::TextDisabled("Docking is enabled: drag a window onto another to dock it.");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::End();

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

} // namespace IKore
