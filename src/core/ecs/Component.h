#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * @file Component.h
 * @brief Type-erased component metadata for the data-oriented ECS.
 *
 * Part of Milestone 8 (issue #140). Components in this ECS are plain value types
 * stored in contiguous, type-erased columns (see Archetype.h). To manage those
 * columns generically we need, per component type: its size and alignment, and
 * a way to move-construct and destroy instances. Those are captured once per
 * type in a ComponentInfo and addressed by a small integer ComponentId.
 */
namespace IKore {
namespace ecs {

/// Dense integer id assigned to each component type the first time it is used.
using ComponentId = std::uint32_t;

/// Sentinel meaning "no component".
inline constexpr ComponentId kInvalidComponent = 0xFFFFFFFFu;

/**
 * @brief Type-erased operations + layout for a single component type.
 *
 * Only move-construction and destruction are needed: columns relocate elements
 * by moving them (never memcpy, so non-trivial components are safe) and destroy
 * them in place. Components must therefore be move-constructible.
 */
struct ComponentInfo {
    std::size_t size{0};
    std::size_t align{0};
    /// Move-construct a new element at @p dst from @p src (src is left moved-from).
    void (*moveConstruct)(void* dst, void* src){nullptr};
    /// Run the destructor of the element at @p p.
    void (*destroy)(void* p){nullptr};
};

/// The ComponentInfo for type @c T (one shared instance per type).
template <class T>
const ComponentInfo& componentInfo() {
    static const ComponentInfo info{
        sizeof(T),
        alignof(T),
        [](void* dst, void* src) { ::new (dst) T(std::move(*static_cast<T*>(src))); },
        [](void* p) { static_cast<T*>(p)->~T(); }};
    return info;
}

/// Global table mapping ComponentId -> ComponentInfo*, populated on first use.
inline std::vector<const ComponentInfo*>& componentRegistry() {
    static std::vector<const ComponentInfo*> registry;
    return registry;
}

/// Stable, process-wide id for component type @c T (assigned lazily, once).
template <class T>
ComponentId componentId() {
    static const ComponentId id = [] {
        const ComponentId newId = static_cast<ComponentId>(componentRegistry().size());
        componentRegistry().push_back(&componentInfo<T>());
        return newId;
    }();
    return id;
}

/// Look up the ComponentInfo for a previously-assigned id.
inline const ComponentInfo* componentInfoById(ComponentId id) {
    return componentRegistry()[id];
}

} // namespace ecs
} // namespace IKore
