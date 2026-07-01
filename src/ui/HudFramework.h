#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @file HudFramework.h
 * @brief Reusable in-game UI/HUD framework: anchoring + live data binding (issue #55).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer-agnostic core
 * behind the in-game HUD and menu screens (#58): a set of HUD elements, each
 * anchored to a screen corner/edge/center with a pixel offset, bound to a live
 * value source (a getter) so it reflects ECS/game state without any per-frame
 * copying. Layout is resolution aware and scale aware, so the same HUD lays out
 * correctly at any framebuffer size and honors the UI scaling work (#61).
 *
 * The ImGui panel in DebugUI walks elements() and draws each one at positionOf();
 * keeping the anchoring math and the bindings here makes them unit-testable
 * without a GL context. Header-only, std only (no ImGui, no glm, no ECS types).
 */
namespace IKore {

/// Where an element is anchored within the screen rectangle (origin top-left).
enum class HudAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

/// Which kind of readout an element draws.
enum class HudWidget {
    Text,  ///< a text label / string readout (e.g. a score line)
    Value, ///< an integer readout (e.g. an ammo count)
    Bar,   ///< a normalized 0..1 bar (e.g. a health fraction)
    List   ///< a list of strings (e.g. an inventory)
};

struct HudVec2 {
    float x{0.0f};
    float y{0.0f};
};

inline const char* hudAnchorName(HudAnchor anchor) {
    switch (anchor) {
        case HudAnchor::TopLeft: return "TopLeft";
        case HudAnchor::TopCenter: return "TopCenter";
        case HudAnchor::TopRight: return "TopRight";
        case HudAnchor::MiddleLeft: return "MiddleLeft";
        case HudAnchor::Center: return "Center";
        case HudAnchor::MiddleRight: return "MiddleRight";
        case HudAnchor::BottomLeft: return "BottomLeft";
        case HudAnchor::BottomCenter: return "BottomCenter";
        case HudAnchor::BottomRight: return "BottomRight";
    }
    return "Unknown";
}

/**
 * @brief Resolve the top-left pixel position of an anchored element.
 * @param anchor       Which screen corner/edge/center to anchor to.
 * @param screen       Screen (framebuffer) size in pixels.
 * @param elementSize  Element size in pixels (used for center/right/bottom anchors).
 * @param offset       Margin, in pixels, from the anchored edge (edge anchors) or a
 *                     shift from center (center anchors). Positive values push inward.
 * @return The element's top-left position in pixels.
 *
 * Pure layout math (no scale here; callers pre-scale size and offset). For edge
 * anchors the offset is a margin from that edge, so a right/bottom offset moves the
 * element inward, keeping HUD margins symmetric regardless of resolution.
 */
inline HudVec2 resolveAnchor(HudAnchor anchor, HudVec2 screen, HudVec2 elementSize, HudVec2 offset) {
    float px = 0.0f;
    switch (anchor) {
        case HudAnchor::TopLeft:
        case HudAnchor::MiddleLeft:
        case HudAnchor::BottomLeft:
            px = offset.x;
            break;
        case HudAnchor::TopCenter:
        case HudAnchor::Center:
        case HudAnchor::BottomCenter:
            px = (screen.x - elementSize.x) * 0.5f + offset.x;
            break;
        case HudAnchor::TopRight:
        case HudAnchor::MiddleRight:
        case HudAnchor::BottomRight:
            px = screen.x - elementSize.x - offset.x;
            break;
    }

    float py = 0.0f;
    switch (anchor) {
        case HudAnchor::TopLeft:
        case HudAnchor::TopCenter:
        case HudAnchor::TopRight:
            py = offset.y;
            break;
        case HudAnchor::MiddleLeft:
        case HudAnchor::Center:
        case HudAnchor::MiddleRight:
            py = (screen.y - elementSize.y) * 0.5f + offset.y;
            break;
        case HudAnchor::BottomLeft:
        case HudAnchor::BottomCenter:
        case HudAnchor::BottomRight:
            py = screen.y - elementSize.y - offset.y;
            break;
    }

    return {px, py};
}

/**
 * @brief A single HUD element: an anchored, live-bound widget.
 *
 * Only the data source matching @ref widget is consulted; the accessors return
 * safe defaults when a source is unbound. Bind a source to a getter that reads
 * live game/ECS state (see the hud* helper builders) so the readout updates every
 * frame without the HUD owning or copying that state.
 *
 * A plain aggregate (public members, no user constructors) so callers can also
 * build one field by field.
 */
struct HudElement {
    std::string name;
    HudAnchor anchor{HudAnchor::TopLeft};
    HudVec2 offset{};        ///< pixels from the anchor, pre-scale
    HudVec2 size{};          ///< element size in pixels, pre-scale
    HudWidget widget{HudWidget::Text};
    bool visible{true};

    std::function<std::string()> textSource;
    std::function<int()> valueSource;
    std::function<float()> barSource;
    std::function<std::vector<std::string>()> listSource;

    /// Current string readout (empty if unbound).
    std::string text() const { return textSource ? textSource() : std::string(); }

    /// Current integer readout (0 if unbound).
    int value() const { return valueSource ? valueSource() : 0; }

    /// Current bar fraction, clamped to [0, 1] (0 if unbound).
    float bar() const {
        float v = barSource ? barSource() : 0.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return v;
    }

    /// Current list contents (empty if unbound).
    std::vector<std::string> list() const {
        return listSource ? listSource() : std::vector<std::string>();
    }
};

/// Build a text readout element bound to @p source.
inline HudElement hudText(std::string name, HudAnchor anchor, HudVec2 offset, HudVec2 size,
                          std::function<std::string()> source) {
    HudElement e;
    e.name = std::move(name);
    e.anchor = anchor;
    e.offset = offset;
    e.size = size;
    e.widget = HudWidget::Text;
    e.textSource = std::move(source);
    return e;
}

/// Build an integer readout element bound to @p source.
inline HudElement hudValue(std::string name, HudAnchor anchor, HudVec2 offset, HudVec2 size,
                           std::function<int()> source) {
    HudElement e;
    e.name = std::move(name);
    e.anchor = anchor;
    e.offset = offset;
    e.size = size;
    e.widget = HudWidget::Value;
    e.valueSource = std::move(source);
    return e;
}

/// Build a normalized (0..1) bar element bound to @p source.
inline HudElement hudBar(std::string name, HudAnchor anchor, HudVec2 offset, HudVec2 size,
                         std::function<float()> source) {
    HudElement e;
    e.name = std::move(name);
    e.anchor = anchor;
    e.offset = offset;
    e.size = size;
    e.widget = HudWidget::Bar;
    e.barSource = std::move(source);
    return e;
}

/// Build a string-list element (e.g. an inventory) bound to @p source.
inline HudElement hudList(std::string name, HudAnchor anchor, HudVec2 offset, HudVec2 size,
                          std::function<std::vector<std::string>()> source) {
    HudElement e;
    e.name = std::move(name);
    e.anchor = anchor;
    e.offset = offset;
    e.size = size;
    e.widget = HudWidget::List;
    e.listSource = std::move(source);
    return e;
}

/**
 * @brief A collection of HUD elements laid out against a screen size and scale.
 *
 * The renderer sets the current screen (framebuffer) size and UI scale each frame,
 * then walks elements() and draws each at positionOf(). Menu screens reuse the
 * same container rather than hand-positioning per screen.
 */
class Hud {
public:
    void setScreenSize(float width, float height) {
        m_screen.x = width;
        m_screen.y = height;
    }
    HudVec2 screenSize() const { return m_screen; }

    /// Set the UI scale factor (#61). Non-positive values are ignored.
    void setScale(float scale) {
        if (scale > 0.0f) m_scale = scale;
    }
    float scale() const { return m_scale; }

    /// Append an element and return a reference to it. Elements are held in a deque
    /// so this reference stays valid as more elements are added, letting callers keep
    /// handles to bind or toggle later.
    HudElement& add(HudElement element) {
        m_elements.push_back(std::move(element));
        return m_elements.back();
    }

    void clear() { m_elements.clear(); }
    std::size_t count() const { return m_elements.size(); }
    const std::deque<HudElement>& elements() const { return m_elements; }
    std::deque<HudElement>& elements() { return m_elements; }

    /// Top-left pixel position of @p element for the current screen size and scale.
    HudVec2 positionOf(const HudElement& element) const {
        const HudVec2 scaledSize{element.size.x * m_scale, element.size.y * m_scale};
        const HudVec2 scaledOffset{element.offset.x * m_scale, element.offset.y * m_scale};
        return resolveAnchor(element.anchor, m_screen, scaledSize, scaledOffset);
    }

private:
    HudVec2 m_screen{1280.0f, 720.0f};
    float m_scale{1.0f};
    std::deque<HudElement> m_elements;
};

} // namespace IKore
