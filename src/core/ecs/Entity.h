#pragma once

#include <cstdint>
#include <functional>

/**
 * @file Entity.h
 * @brief Stable, generational entity handle for the data-oriented ECS.
 *
 * Part of Milestone 8 (issue #140). An Entity is a lightweight value type —
 * an index plus a generation counter — that stays valid regardless of where
 * the entity's component data physically lives. When an entity is destroyed
 * its index is recycled with a bumped generation, so stale handles referring
 * to the old occupant compare as invalid.
 */
namespace IKore {
namespace ecs {

/**
 * @brief A handle to an entity. Cheap to copy and store.
 *
 * The handle does not point at component storage directly; the Registry maps it
 * to the current archetype/row. This indirection is what keeps handles stable
 * while the underlying SoA columns are repacked.
 */
struct Entity {
    std::uint32_t index{0};      ///< Slot index into the Registry's record table.
    std::uint32_t generation{0}; ///< Bumped each time the slot is recycled.

    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

    /// An explicitly invalid handle.
    static constexpr Entity invalid() { return Entity{kInvalidIndex, 0}; }

    bool isValid() const { return index != kInvalidIndex; }

    bool operator==(const Entity& o) const {
        return index == o.index && generation == o.generation;
    }
    bool operator!=(const Entity& o) const { return !(*this == o); }
};

} // namespace ecs
} // namespace IKore

namespace std {
template <>
struct hash<IKore::ecs::Entity> {
    std::size_t operator()(const IKore::ecs::Entity& e) const noexcept {
        return (static_cast<std::size_t>(e.generation) << 32) ^ e.index;
    }
};
} // namespace std
