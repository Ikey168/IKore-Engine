#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @file EntityInspector.h
 * @brief Entity inspector model: component reflection + selection (issue #56).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer-agnostic core
 * behind the ImGui entity inspector: a small reflection layer that describes a
 * component's editable fields as typed get/set properties, plus a selection model
 * that caches the selected entity's component views so the panel does no per-frame
 * allocation - it rebuilds only when the selection changes (or on refresh()).
 *
 * The core is deliberately ECS-agnostic: it takes a Builder that maps an entity id
 * to its component views, so it can reflect the archetype ECS (see EcsInspector.h)
 * or any other object graph, and is unit-testable without a GL context. Editing a
 * property calls its setter, which writes straight into the live component, so
 * changes apply immediately in the running scene. Header-only, std only (no ImGui,
 * no glm, no ECS types).
 */
namespace IKore {

/// The value kind a property holds, so the panel can pick the right widget.
enum class PropertyType { Bool, Int, Float, Float3, String };

/// A glm-free 3-vector for reflecting position/rotation/scale style fields.
struct Vec3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

/**
 * @brief One editable field of a component: a name, a type, and typed accessors.
 *
 * Only the getter/setter pair matching @ref type is populated; the others stay
 * empty. The getter reads the live value and the setter writes it straight back,
 * so an edit in the panel is reflected in the running scene on the same frame.
 * The min/max/speed hints drive numeric drag widgets (min == max means unbounded).
 * A plain aggregate; use the prop* helpers to build the common cases.
 */
struct Property {
    std::string name;
    PropertyType type{PropertyType::Float};

    std::function<bool()> getBool;
    std::function<void(bool)> setBool;
    std::function<int()> getInt;
    std::function<void(int)> setInt;
    std::function<float()> getFloat;
    std::function<void(float)> setFloat;
    std::function<Vec3f()> getVec3;
    std::function<void(Vec3f)> setVec3;
    std::function<std::string()> getString;
    std::function<void(std::string)> setString;

    float minValue{0.0f};
    float maxValue{0.0f};
    float speed{0.1f};
};

inline Property propBool(std::string name, std::function<bool()> get, std::function<void(bool)> set) {
    Property p;
    p.name = std::move(name);
    p.type = PropertyType::Bool;
    p.getBool = std::move(get);
    p.setBool = std::move(set);
    return p;
}

inline Property propInt(std::string name, std::function<int()> get, std::function<void(int)> set,
                        float speed = 1.0f, float minValue = 0.0f, float maxValue = 0.0f) {
    Property p;
    p.name = std::move(name);
    p.type = PropertyType::Int;
    p.getInt = std::move(get);
    p.setInt = std::move(set);
    p.speed = speed;
    p.minValue = minValue;
    p.maxValue = maxValue;
    return p;
}

inline Property propFloat(std::string name, std::function<float()> get, std::function<void(float)> set,
                          float speed = 0.1f, float minValue = 0.0f, float maxValue = 0.0f) {
    Property p;
    p.name = std::move(name);
    p.type = PropertyType::Float;
    p.getFloat = std::move(get);
    p.setFloat = std::move(set);
    p.speed = speed;
    p.minValue = minValue;
    p.maxValue = maxValue;
    return p;
}

inline Property propFloat3(std::string name, std::function<Vec3f()> get, std::function<void(Vec3f)> set,
                           float speed = 0.1f) {
    Property p;
    p.name = std::move(name);
    p.type = PropertyType::Float3;
    p.getVec3 = std::move(get);
    p.setVec3 = std::move(set);
    p.speed = speed;
    return p;
}

inline Property propString(std::string name, std::function<std::string()> get,
                           std::function<void(std::string)> set) {
    Property p;
    p.name = std::move(name);
    p.type = PropertyType::String;
    p.getString = std::move(get);
    p.setString = std::move(set);
    return p;
}

/// A named group of properties, corresponding to one component on an entity.
struct ComponentView {
    std::string name;
    std::vector<Property> properties;
};

/**
 * @brief Selection model + cached component reflection for the inspector panel.
 *
 * Point it at a Builder (entity id -> component views), then drive selection from
 * the scene hierarchy (#59) or viewport picking (#57) via select(). The selected
 * entity's component views are built once on selection change and cached, so the
 * panel iterates components() every frame with no rebuild or allocation; call
 * refresh() when the entity's component set changes (e.g. a component is added).
 */
class EntityInspector {
public:
    using EntityId = std::uint64_t;
    static constexpr EntityId kNoEntity = ~EntityId{0};

    /// Maps an entity id to the component views to show for it.
    using Builder = std::function<std::vector<ComponentView>(EntityId)>;

    /// Set the reflection source. Rebuilds the current selection, if any.
    void setBuilder(Builder builder) {
        m_builder = std::move(builder);
        if (m_hasSelection) rebuild();
    }

    /// Select an entity; rebuilds its component views only if the id changed.
    void select(EntityId id) {
        if (m_hasSelection && id == m_selected) return;
        m_selected = id;
        m_hasSelection = true;
        rebuild();
    }

    /// Clear the selection and its cached views.
    void deselect() {
        m_hasSelection = false;
        m_selected = kNoEntity;
        m_components.clear();
    }

    /// Force a rebuild of the current selection (e.g. after components change).
    void refresh() {
        if (m_hasSelection) rebuild();
    }

    bool hasSelection() const { return m_hasSelection; }
    EntityId selected() const { return m_selected; }

    /// The selected entity's component views (empty if nothing is selected).
    /// Cached: reading this does not allocate or rebuild.
    const std::vector<ComponentView>& components() const { return m_components; }

    /// How many times the views were rebuilt - for tests asserting the cache holds.
    std::size_t rebuildCount() const { return m_rebuildCount; }

private:
    void rebuild() {
        if (m_builder && m_hasSelection) {
            m_components = m_builder(m_selected);
        } else {
            m_components.clear();
        }
        ++m_rebuildCount;
    }

    Builder m_builder;
    std::vector<ComponentView> m_components;
    EntityId m_selected{kNoEntity};
    bool m_hasSelection{false};
    std::size_t m_rebuildCount{0};
};

} // namespace IKore
