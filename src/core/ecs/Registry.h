#pragma once

#include "core/ecs/Archetype.h"
#include "core/ecs/Component.h"
#include "core/ecs/Entity.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

/**
 * @file Registry.h
 * @brief The ECS world: owns entities, archetypes, and the entity->location map.
 *
 * Part of Milestone 8 (issue #140). The Registry is the public entry point for
 * the data-oriented ECS. It hands out stable Entity handles, stores components
 * in archetype-grouped SoA columns, and moves entities between archetypes as
 * components are added or removed.
 *
 * Scope note: this milestone delivers storage + stable handles + add/remove/get.
 * The cache-friendly iteration/query API lives in View.h (issue #141); migrating
 * the engine's existing components onto this storage is issue #142.
 */
namespace IKore {
namespace ecs {

// Defined in View.h: the query/iteration API (issue #141).
template <class... Include>
class View;

class Registry {
public:
    Registry() { m_empty = getOrCreate({}); }

    // Non-copyable: it owns raw-pointer-stable archetypes and aligned buffers.
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /// Create a fresh entity (initially with no components).
    Entity create() {
        std::uint32_t index;
        if (!m_freeList.empty()) {
            index = m_freeList.back();
            m_freeList.pop_back();
        } else {
            index = static_cast<std::uint32_t>(m_records.size());
            m_records.emplace_back();
        }
        Record& record = m_records[index];
        record.alive = true;
        record.archetype = m_empty;
        record.row = static_cast<std::uint32_t>(m_empty->entities.size());
        m_empty->entities.push_back(index);
        return Entity{index, record.generation};
    }

    /// True if @p e refers to a currently-live entity (generation matches).
    bool isValid(Entity e) const {
        return e.index < m_records.size() && m_records[e.index].alive &&
               m_records[e.index].generation == e.generation;
    }

    /// Destroy @p e and recycle its slot (bumping the generation).
    void destroy(Entity e) {
        if (!isValid(e)) return;
        Record& record = m_records[e.index];
        const std::uint32_t relocated = record.archetype->removeRow(record.row);
        if (relocated != Entity::kInvalidIndex) m_records[relocated].row = record.row;
        record.alive = false;
        record.archetype = nullptr;
        ++record.generation;
        m_freeList.push_back(e.index);
    }

    template <class T>
    bool has(Entity e) const {
        return isValid(e) && m_records[e.index].archetype->has(componentId<T>());
    }

    /// Add (or overwrite) component @c T on @p e; returns a reference to it.
    template <class T>
    T& add(Entity e, T value = {}) {
        const ComponentId cid = componentId<T>();
        Record& record = m_records[e.index];
        Archetype* from = record.archetype;

        const int existing = from->columnIndex(cid);
        if (existing >= 0) {
            T& slot = *static_cast<T*>(from->columns[existing].at(record.row));
            slot = std::move(value);
            return slot;
        }

        std::vector<ComponentId> signature = from->signature;
        signature.push_back(cid);
        std::sort(signature.begin(), signature.end());
        Archetype* to = getOrCreate(signature);

        moveRow(e.index, from, to, /*addId=*/cid, /*addSrc=*/&value);
        return get<T>(e);
    }

    /// Remove component @c T from @p e (no-op if absent).
    template <class T>
    void remove(Entity e) {
        const ComponentId cid = componentId<T>();
        Record& record = m_records[e.index];
        Archetype* from = record.archetype;
        if (from->columnIndex(cid) < 0) return;

        std::vector<ComponentId> signature;
        signature.reserve(from->signature.size() - 1);
        for (ComponentId id : from->signature) {
            if (id != cid) signature.push_back(id);
        }
        Archetype* to = getOrCreate(signature);
        // The removed component's data lives only in `from`; moveRow copies the
        // surviving columns and the swap-remove on `from` destroys the rest.
        moveRow(e.index, from, to, /*addId=*/kInvalidComponent, /*addSrc=*/nullptr);
    }

    /// Reference to @p e's component @c T. Precondition: has<T>(e).
    template <class T>
    T& get(Entity e) {
        Record& record = m_records[e.index];
        const int col = record.archetype->columnIndex(componentId<T>());
        return *static_cast<T*>(record.archetype->columns[col].at(record.row));
    }

    template <class T>
    const T& get(Entity e) const {
        const Record& record = m_records[e.index];
        const int col = record.archetype->columnIndex(componentId<T>());
        return *static_cast<const T*>(record.archetype->columns[col].at(record.row));
    }

    /// Number of currently-live entities.
    std::size_t aliveCount() const { return m_records.size() - m_freeList.size(); }

    /// Number of distinct archetypes (including the empty one). Mostly for tests.
    std::size_t archetypeCount() const { return m_archetypes.size(); }

    /// Begin a query over all entities that have every one of @c Include.
    /// Compose with `.exclude<...>()` and iterate with `.each(fn)`; see View.h.
    template <class... Include>
    View<Include...> view();

private:
    template <class...> friend class View;

    struct Record {
        std::uint32_t generation{0};
        bool alive{false};
        Archetype* archetype{nullptr};
        std::uint32_t row{0};
    };

    Archetype* getOrCreate(std::vector<ComponentId> signature) {
        auto it = m_index.find(signature);
        if (it != m_index.end()) return it->second;

        auto archetype = std::make_unique<Archetype>();
        archetype->signature = signature;
        archetype->columns.reserve(signature.size());
        for (ComponentId id : signature) {
            archetype->columns.emplace_back(componentInfoById(id));
        }
        Archetype* ptr = archetype.get();
        m_index.emplace(std::move(signature), ptr);
        m_archetypes.push_back(std::move(archetype));
        return ptr;
    }

    /**
     * @brief Relocate entity @p index from archetype @p from to @p to.
     *
     * Surviving components are moved column-by-column; if @p addId is valid the
     * new component is move-constructed from @p addSrc. The source row is then
     * swap-removed from @p from, and any entity relocated by that swap has its
     * record patched.
     */
    void moveRow(std::uint32_t index, Archetype* from, Archetype* to, ComponentId addId,
                 void* addSrc) {
        Record& record = m_records[index];
        const std::size_t srcRow = record.row;

        for (std::size_t i = 0; i < to->signature.size(); ++i) {
            const ComponentId id = to->signature[i];
            Column& dst = to->columns[i];
            if (id == addId) {
                dst.pushMove(addSrc);
            } else {
                const int fromCol = from->columnIndex(id);
                dst.pushMove(from->columns[fromCol].at(srcRow));
            }
        }
        const auto newRow = static_cast<std::uint32_t>(to->entities.size());
        to->entities.push_back(index);

        const std::uint32_t relocated = from->removeRow(srcRow);
        if (relocated != Entity::kInvalidIndex) {
            m_records[relocated].row = static_cast<std::uint32_t>(srcRow);
        }

        record.archetype = to;
        record.row = newRow;
    }

    std::vector<Record> m_records;
    std::vector<std::uint32_t> m_freeList;
    std::vector<std::unique_ptr<Archetype>> m_archetypes;
    std::map<std::vector<ComponentId>, Archetype*> m_index;
    Archetype* m_empty{nullptr};
};

} // namespace ecs
} // namespace IKore
