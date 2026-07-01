#include "ui/DebugUI.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/Logger.h"

#include <string>
#include <vector>

namespace IKore {

// File-static trampoline: ImGui's InputText callback dispatches here, then into
// the DebugUI instance passed as user data.
static int consoleTextEditStub(ImGuiInputTextCallbackData* data) {
    return static_cast<DebugUI*>(data->UserData)->onConsoleTextEdit(data);
}

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

    m_console.registerCommand("version", "print the engine name", [](const std::vector<std::string>&) {
        return std::string("IKore Engine");
    });
    m_console.print("Console ready. Type 'help'.");

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

void DebugUI::update(float deltaTimeSeconds) {
    // Cheap frame-time recording; runs even while the overlay is hidden so the
    // graph is populated when it is opened.
    m_perf.record(deltaTimeSeconds);
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
    ImGui::Checkbox("Show performance overlay (#53)", &m_showPerf);
    ImGui::Checkbox("Show console (#54)", &m_showConsole);
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::End();

    renderPerfOverlay();
    renderConsole();

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void DebugUI::renderConsole() {
    if (!m_showConsole) return;

    ImGui::Begin("Console", &m_showConsole);

    // Scrollback region, leaving room for the input line at the bottom.
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("console_scroll", ImVec2(0.0f, -footerHeight), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const std::string& line : m_console.lines()) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f); // autoscroll
    ImGui::EndChild();

    ImGui::Separator();
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_CallbackCompletion |
                                      ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("Input", m_consoleInput, sizeof(m_consoleInput), flags, &consoleTextEditStub,
                         this)) {
        m_console.execute(m_consoleInput);
        m_consoleInput[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1); // keep focus on the input
    }
    ImGui::End();
}

int DebugUI::onConsoleTextEdit(void* callbackData) {
    ImGuiInputTextCallbackData* data = static_cast<ImGuiInputTextCallbackData*>(callbackData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        const std::string current(data->Buf, data->Buf + data->BufTextLen);
        const std::string completed = m_console.complete(current);
        if (completed.size() > current.size()) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, completed.c_str());
        }
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        std::string recalled;
        if (data->EventKey == ImGuiKey_UpArrow) {
            recalled = m_console.historyPrev();
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            recalled = m_console.historyNext();
        }
        data->DeleteChars(0, data->BufTextLen);
        if (!recalled.empty()) data->InsertChars(0, recalled.c_str());
    }
    return 0;
}

void DebugUI::renderPerfOverlay() {
    if (!m_showPerf) return;

    m_perf.refreshMemory(); // sampled only while the overlay is on screen

    ImGui::Begin("Performance", &m_showPerf);
    ImGui::Text("FPS: %.1f  (avg %.1f)", m_perf.fps(), m_perf.avgFps());
    ImGui::Text("Frame: %.3f ms  (avg %.3f, min %.3f, max %.3f)", static_cast<double>(m_perf.lastFrameMs()),
                static_cast<double>(m_perf.avgFrameMs()), static_cast<double>(m_perf.minFrameMs()),
                static_cast<double>(m_perf.maxFrameMs()));

    const std::vector<float> history = m_perf.historyMs();
    if (!history.empty()) {
        const float scaleMax = m_perf.maxFrameMs() * 1.25f + 1.0f;
        ImGui::PlotLines("Frame ms", history.data(), static_cast<int>(history.size()), 0, nullptr, 0.0f,
                         scaleMax, ImVec2(0.0f, 60.0f));
    }

    const long memoryBytes = m_perf.memoryBytes();
    if (memoryBytes > 0) {
        ImGui::Text("Memory (RSS): %.1f MB", static_cast<double>(memoryBytes) / (1024.0 * 1024.0));
    }
    ImGui::End();
}

} // namespace IKore
