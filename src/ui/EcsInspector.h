#pragma once

#include "core/ecs/ECS.h"
#include "core/ecs/components/Components.h"
#include "ui/EntityInspector.h"

#include <cstdint>
#include <vector>

/**
 * @file EcsInspector.h
 * @brief Bridge the archetype ECS to the entity inspector's reflection (issue #56).
 *
 * Milestone 9 (In-Engine Editor & Tooling). Turns the archetype ECS components
 * from Milestone 8 (Transform, Velocity, RigidBody, AIAgent) into the inspector's
 * ComponentView / Property model, with each property's get/set reading and writing
 * the live component through the Registry, so edits apply immediately in the
 * running scene. Only the components an entity actually has are reflected.
 *
 * Header-only. Depends on the header-only, glm-free ECS and the (equally
 * dependency-free) EntityInspector, so it is unit-testable against a real Registry
 * without any GL context; the ImGui panel in DebugUI draws whatever this produces.
 */
namespace IKore {

/// Pack an ECS entity handle into the inspector's opaque 64-bit id.
inline EntityInspector::EntityId packEntity(ecs::Entity e) {
    return (static_cast<std::uint64_t>(e.generation) << 32) | static_cast<std::uint64_t>(e.index);
}

/// Unpack an inspector id back into an ECS entity handle.
inline ecs::Entity unpackEntity(EntityInspector::EntityId id) {
    return ecs::Entity{static_cast<std::uint32_t>(id & 0xFFFFFFFFu),
                       static_cast<std::uint32_t>(id >> 32)};
}

namespace detail {

/// Bridge an ecs::Vec3 field of component T to a Float3 property.
template <class T>
inline Property vec3Property(const char* name, ecs::Registry& reg, ecs::Entity e,
                             ecs::Vec3 T::*member) {
    return propFloat3(
        name,
        [&reg, e, member] {
            const ecs::Vec3& v = reg.get<T>(e).*member;
            return Vec3f{v.x, v.y, v.z};
        },
        [&reg, e, member](Vec3f v) { reg.get<T>(e).*member = ecs::Vec3{v.x, v.y, v.z}; });
}

/// Bridge a float field of component T to a Float property.
template <class T>
inline Property floatProperty(const char* name, ecs::Registry& reg, ecs::Entity e, float T::*member,
                              float speed = 0.05f, float minValue = 0.0f, float maxValue = 0.0f) {
    return propFloat(
        name, [&reg, e, member] { return reg.get<T>(e).*member; },
        [&reg, e, member](float f) { reg.get<T>(e).*member = f; }, speed, minValue, maxValue);
}

/// Bridge a bool field of component T to a Bool property.
template <class T>
inline Property boolProperty(const char* name, ecs::Registry& reg, ecs::Entity e, bool T::*member) {
    return propBool(
        name, [&reg, e, member] { return reg.get<T>(e).*member; },
        [&reg, e, member](bool b) { reg.get<T>(e).*member = b; });
}

} // namespace detail

inline ComponentView reflectTransform(ecs::Registry& reg, ecs::Entity e) {
    ComponentView cv;
    cv.name = "Transform";
    cv.properties.push_back(detail::vec3Property("position", reg, e, &ecs::Transform::position));
    cv.properties.push_back(detail::vec3Property("rotation", reg, e, &ecs::Transform::rotation));
    cv.properties.push_back(detail::vec3Property("scale", reg, e, &ecs::Transform::scale));
    return cv;
}

inline ComponentView reflectVelocity(ecs::Registry& reg, ecs::Entity e) {
    ComponentView cv;
    cv.name = "Velocity";
    cv.properties.push_back(detail::vec3Property("linear", reg, e, &ecs::Velocity::linear));
    cv.properties.push_back(detail::vec3Property("angular", reg, e, &ecs::Velocity::angular));
    return cv;
}

inline ComponentView reflectRigidBody(ecs::Registry& reg, ecs::Entity e) {
    ComponentView cv;
    cv.name = "RigidBody";
    cv.properties.push_back(detail::vec3Property("acceleration", reg, e, &ecs::RigidBody::acceleration));
    cv.properties.push_back(detail::floatProperty("mass", reg, e, &ecs::RigidBody::mass, 0.05f, 0.0f, 0.0f));
    cv.properties.push_back(
        detail::floatProperty("damping", reg, e, &ecs::RigidBody::damping, 0.01f, 0.0f, 1.0f));
    cv.properties.push_back(detail::boolProperty("kinematic", reg, e, &ecs::RigidBody::kinematic));
    return cv;
}

inline ComponentView reflectAIAgent(ecs::Registry& reg, ecs::Entity e) {
    ComponentView cv;
    cv.name = "AIAgent";
    cv.properties.push_back(detail::vec3Property("target", reg, e, &ecs::AIAgent::target));
    cv.properties.push_back(detail::floatProperty("speed", reg, e, &ecs::AIAgent::speed, 0.05f, 0.0f, 0.0f));
    cv.properties.push_back(
        detail::floatProperty("arriveRadius", reg, e, &ecs::AIAgent::arriveRadius, 0.01f, 0.0f, 0.0f));
    cv.properties.push_back(detail::boolProperty("active", reg, e, &ecs::AIAgent::active));
    return cv;
}

/// Reflect every known component an entity currently has, in a stable order.
inline std::vector<ComponentView> reflectEntity(ecs::Registry& reg, ecs::Entity e) {
    std::vector<ComponentView> views;
    if (!reg.isValid(e)) return views;
    if (reg.has<ecs::Transform>(e)) views.push_back(reflectTransform(reg, e));
    if (reg.has<ecs::Velocity>(e)) views.push_back(reflectVelocity(reg, e));
    if (reg.has<ecs::RigidBody>(e)) views.push_back(reflectRigidBody(reg, e));
    if (reg.has<ecs::AIAgent>(e)) views.push_back(reflectAIAgent(reg, e));
    return views;
}

/// Point an inspector at @p reg: selecting a packed entity id reflects its live
/// components. The registry must outlive the inspector's use of the builder.
inline void installEcsBuilder(EntityInspector& inspector, ecs::Registry& reg) {
    inspector.setBuilder(
        [&reg](EntityInspector::EntityId id) { return reflectEntity(reg, unpackEntity(id)); });
}

} // namespace IKore
