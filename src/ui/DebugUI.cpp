#include "ui/DebugUI.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/Logger.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace IKore {

// File-static trampoline: ImGui's InputText callback dispatches here, then into
// the DebugUI instance passed as user data.
static int consoleTextEditStub(ImGuiInputTextCallbackData* data) {
    return static_cast<DebugUI*>(data->UserData)->onConsoleTextEdit(data);
}

// Draw one reflected property with the widget matching its type; an edit writes
// straight back through the setter, so it applies live to the component (#56).
static void drawProperty(const Property& prop) {
    const char* label = prop.name.c_str();
    switch (prop.type) {
        case PropertyType::Bool: {
            bool v = prop.getBool();
            if (ImGui::Checkbox(label, &v)) prop.setBool(v);
            break;
        }
        case PropertyType::Int: {
            int v = prop.getInt();
            if (ImGui::DragInt(label, &v, prop.speed, static_cast<int>(prop.minValue),
                               static_cast<int>(prop.maxValue)))
                prop.setInt(v);
            break;
        }
        case PropertyType::Float: {
            float v = prop.getFloat();
            if (ImGui::DragFloat(label, &v, prop.speed, prop.minValue, prop.maxValue)) prop.setFloat(v);
            break;
        }
        case PropertyType::Float3: {
            const Vec3f v = prop.getVec3();
            float a[3] = {v.x, v.y, v.z};
            if (ImGui::DragFloat3(label, a, prop.speed)) prop.setVec3(Vec3f{a[0], a[1], a[2]});
            break;
        }
        case PropertyType::String: {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s", prop.getString().c_str());
            if (ImGui::InputText(label, buf, sizeof(buf))) prop.setString(std::string(buf));
            break;
        }
    }
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

    // Demo HUD wired through the reusable framework (#55). Each widget is anchored
    // and bound to a live getter; in a real game these would read ECS component
    // values. update() animates the demo state so the readouts visibly change.
    m_demoInventory = {"Sword", "Shield", "Potion x3", "Map"};
    m_hud.add(hudBar("Health", HudAnchor::TopLeft, {16.0f, 16.0f}, {220.0f, 20.0f},
                     [this] { return m_demoHealth; }));
    m_hud.add(hudText("Score", HudAnchor::TopRight, {16.0f, 16.0f}, {200.0f, 24.0f},
                      [this] { return std::string("Score: ") + std::to_string(m_demoScore); }));
    m_hud.add(hudValue("Ammo", HudAnchor::BottomRight, {16.0f, 16.0f}, {140.0f, 24.0f},
                       [this] { return m_demoAmmo; }));
    m_hud.add(hudList("Inventory", HudAnchor::BottomLeft, {16.0f, 16.0f}, {180.0f, 130.0f},
                      [this] { return m_demoInventory; }));

    // Demo ECS scene for the entity inspector (#56). A few archetype-ECS entities
    // with different component sets; the inspector reflects and live-edits them.
    {
        ecs::Entity player = m_demoRegistry.create();
        m_demoRegistry.add<ecs::Transform>(player,
                                           ecs::Transform{{0.0f, 1.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
        m_demoRegistry.add<ecs::Velocity>(player, ecs::Velocity{{1.0f, 0.0f, 0.0f}, {}});
        m_demoRegistry.add<ecs::RigidBody>(player,
                                           ecs::RigidBody{{0.0f, -9.8f, 0.0f}, 1.0f, 0.05f, false});
        m_demoEntities.push_back(player);

        ecs::Entity enemy = m_demoRegistry.create();
        m_demoRegistry.add<ecs::Transform>(enemy,
                                           ecs::Transform{{5.0f, 0.0f, 2.0f}, {}, {1.0f, 1.0f, 1.0f}});
        m_demoRegistry.add<ecs::AIAgent>(enemy, ecs::AIAgent{{0.0f, 0.0f, 0.0f}, 2.0f, 0.1f, true});
        m_demoEntities.push_back(enemy);

        ecs::Entity prop = m_demoRegistry.create();
        m_demoRegistry.add<ecs::Transform>(prop,
                                           ecs::Transform{{-3.0f, 0.0f, -1.0f}, {}, {2.0f, 2.0f, 2.0f}});
        m_demoEntities.push_back(prop);
    }
    installEcsBuilder(m_inspector, m_demoRegistry);
    if (!m_demoEntities.empty()) m_inspector.select(packEntity(m_demoEntities.front()));

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

    // Animate the demo HUD state so the data-bound widgets visibly update. In a
    // real game these values come from ECS/game systems instead.
    m_hudClock += deltaTimeSeconds;
    m_demoHealth = 0.5f + 0.5f * std::sin(m_hudClock);       // oscillate across 0..1
    m_demoScore = static_cast<int>(m_hudClock * 100.0f);     // climbs over time
    m_demoAmmo = 30 - static_cast<int>(m_hudClock) % 31;     // counts down 30..0, loops
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
    ImGui::Checkbox("Show HUD (#55)", &m_showHud);
    ImGui::SliderFloat("HUD scale (#61)", &m_hudScale, 0.5f, 2.0f, "%.2f");
    ImGui::Checkbox("Show entity inspector (#56)", &m_showInspector);
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::End();

    renderPerfOverlay();
    renderConsole();
    renderHud();
    renderInspector();

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

void DebugUI::renderInspector() {
    if (!m_showInspector) return;

    ImGui::Begin("Entity Inspector", &m_showInspector);

    // Entity picker. Selection will also be driven by viewport picking (#57) and
    // the scene hierarchy (#59) once those land, via inspector().select(id).
    ImGui::TextUnformatted("Entities");
    if (ImGui::BeginListBox("##entities", ImVec2(-1.0f, 90.0f))) {
        for (const ecs::Entity entity : m_demoEntities) {
            const EntityInspector::EntityId id = packEntity(entity);
            const bool selected = m_inspector.hasSelection() && m_inspector.selected() == id;
            char label[32];
            std::snprintf(label, sizeof(label), "Entity %u", entity.index);
            if (ImGui::Selectable(label, selected)) m_inspector.select(id);
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();

    if (!m_inspector.hasSelection() || m_inspector.components().empty()) {
        ImGui::TextDisabled("No entity selected, or it has no components.");
        ImGui::End();
        return;
    }

    // Cached component views: iterated every frame with no rebuild/allocation.
    int componentIndex = 0;
    for (const ComponentView& view : m_inspector.components()) {
        ImGui::PushID(componentIndex++);
        if (ImGui::CollapsingHeader(view.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const Property& prop : view.properties) {
                drawProperty(prop);
            }
        }
        ImGui::PopID();
    }

    ImGui::End();
}

void DebugUI::renderHud() {
    if (!m_showHud) return;

    const ImGuiIO& io = ImGui::GetIO();
    m_hud.setScreenSize(io.DisplaySize.x, io.DisplaySize.y); // resolution aware (#55)
    m_hud.setScale(m_hudScale);                              // scale aware (#61)

    // Borderless, click-through overlays so the HUD never obstructs gameplay input.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    int index = 0;
    for (const HudElement& e : m_hud.elements()) {
        if (!e.visible) continue;

        const HudVec2 pos = m_hud.positionOf(e);
        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y));
        ImGui::SetNextWindowSize(ImVec2(e.size.x * m_hud.scale(), e.size.y * m_hud.scale()));
        ImGui::SetNextWindowBgAlpha(0.35f);

        char windowId[32];
        std::snprintf(windowId, sizeof(windowId), "##hud_%d", index++);
        if (ImGui::Begin(windowId, nullptr, flags)) {
            switch (e.widget) {
                case HudWidget::Text:
                    ImGui::TextUnformatted(e.text().c_str());
                    break;
                case HudWidget::Value:
                    ImGui::Text("%s: %d", e.name.c_str(), e.value());
                    break;
                case HudWidget::Bar: {
                    char label[48];
                    std::snprintf(label, sizeof(label), "%s %.0f%%", e.name.c_str(),
                                  static_cast<double>(e.bar()) * 100.0);
                    ImGui::ProgressBar(e.bar(), ImVec2(-1.0f, 0.0f), label);
                    break;
                }
                case HudWidget::List:
                    ImGui::TextUnformatted((e.name + ":").c_str());
                    for (const std::string& item : e.list()) {
                        ImGui::BulletText("%s", item.c_str());
                    }
                    break;
            }
        }
        ImGui::End();
    }
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
