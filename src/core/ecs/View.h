#pragma once

#include "core/ecs/Archetype.h"
#include "core/ecs/Component.h"
#include "core/ecs/Entity.h"
#include "core/ecs/Registry.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * @file View.h
 * @brief Cache-friendly query/iteration API over archetype storage.
 *
 * Milestone 8 (issue #141). A View selects every entity whose archetype contains
 * all of the requested @c Include components (and, optionally, none of the
 * excluded ones) and iterates them archetype-by-archetype, row-by-row, over the
 * contiguous SoA columns. Component pointers are resolved once per archetype, so
 * the inner loop is a straight walk of packed arrays with no per-entity hashing
 * and no virtual dispatch.
 *
 * Usage:
 * @code
 * world.view<Position, Velocity>().each([](Entity e, Position& p, Velocity& v) {
 *     p.x += v.dx;
 * });
 *
 * // Composed include/exclude:
 * world.view<Position>().exclude<Frozen>().each([](Entity, Position& p) { ... });
 * @endcode
 *
 * Iteration order is deterministic: archetypes in creation order, rows in row
 * order.
 */
namespace IKore {
namespace ecs {

template <class... Include>
class View {
    static_assert(sizeof...(Include) >= 1, "A view needs at least one component type");

public:
    explicit View(Registry& registry) : m_registry(registry) {}

    /// Narrow the query to entities that have NONE of @c Exclude. Chainable.
    template <class... Exclude>
    View& exclude() {
        (m_excludes.push_back(componentId<Exclude>()), ...);
        return *this;
    }

    /**
     * @brief Invoke @p fn(Entity, Include&...) for every matching entity.
     *
     * Components are passed by reference, so @p fn may mutate them in place.
     */
    template <class Fn>
    void each(Fn&& fn) {
        const ComponentId includeIds[] = {componentId<Include>()...};

        for (const auto& archetypePtr : m_registry.m_archetypes) {
            Archetype& archetype = *archetypePtr;

            // Resolve the column for each required component once per archetype;
            // skip the whole archetype if any is missing.
            Column* columns[sizeof...(Include)];
            bool matches = true;
            for (std::size_t i = 0; i < sizeof...(Include); ++i) {
                const int columnIndex = archetype.columnIndex(includeIds[i]);
                if (columnIndex < 0) {
                    matches = false;
                    break;
                }
                columns[i] = &archetype.columns[columnIndex];
            }
            if (!matches) continue;

            bool excluded = false;
            for (ComponentId id : m_excludes) {
                if (archetype.has(id)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            for (std::size_t row = 0; row < archetype.entities.size(); ++row) {
                const std::uint32_t index = archetype.entities[row];
                const Entity entity{index, m_registry.m_records[index].generation};
                invokeRow(fn, entity, columns, row, std::index_sequence_for<Include...>{});
            }
        }
    }

    /// Number of entities the query matches.
    std::size_t count() {
        std::size_t total = 0;
        each([&total](Entity, Include&...) { ++total; });
        return total;
    }

private:
    template <class Fn, std::size_t... I>
    static void invokeRow(Fn& fn, Entity entity, Column** columns, std::size_t row,
                          std::index_sequence<I...>) {
        fn(entity, *static_cast<Include*>(columns[I]->at(row))...);
    }

    Registry& m_registry;
    std::vector<ComponentId> m_excludes;
};

// Defined here, now that View is complete (declared in Registry.h).
template <class... Include>
View<Include...> Registry::view() {
    return View<Include...>(*this);
}

} // namespace ecs
} // namespace IKore
