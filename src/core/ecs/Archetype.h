#pragma once

#include "core/ecs/Component.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

/**
 * @file Archetype.h
 * @brief Contiguous (SoA) component storage grouped by component signature.
 *
 * Part of Milestone 8 (issue #140). All entities that share the exact same set
 * of component types live in one Archetype. Within an archetype each component
 * type gets its own contiguous Column, and a parallel @c entities array records
 * which entity owns each row. This is the structure-of-arrays layout that makes
 * iteration cache-friendly.
 *
 * Rows are kept densely packed: removing a row swaps the last row into the gap
 * (swap-remove), so columns never develop holes.
 */
namespace IKore {
namespace ecs {

/**
 * @brief A type-erased, growable, contiguous array of one component type.
 *
 * Owns an aligned byte buffer and the live elements within it. Move-only; copy
 * is disabled because the type-erased payload cannot be safely copied here.
 */
class Column {
public:
    Column() = default;
    explicit Column(const ComponentInfo* info) : m_info(info) {}

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    Column(Column&& other) noexcept { swap(other); }
    Column& operator=(Column&& other) noexcept {
        if (this != &other) {
            destroyAll();
            release();
            swap(other);
        }
        return *this;
    }

    ~Column() {
        destroyAll();
        release();
    }

    std::size_t size() const { return m_count; }
    const ComponentInfo* info() const { return m_info; }

    /// Pointer to the element at @p row (no bounds checking).
    void* at(std::size_t row) const { return m_data + row * m_info->size; }

    /**
     * @brief Move-construct a new element at the end from @p src.
     * @return the row index of the appended element.
     *
     * @p src is left in a moved-from state; the caller still owns and must
     * destroy it (directly or via its own column's removal).
     */
    std::size_t pushMove(void* src) {
        reserve(m_count + 1);
        m_info->moveConstruct(at(m_count), src);
        return m_count++;
    }

    /// Swap-remove the element at @p row, keeping the column densely packed.
    void removeSwap(std::size_t row) {
        const std::size_t last = m_count - 1;
        m_info->destroy(at(row));
        if (row != last) {
            m_info->moveConstruct(at(row), at(last));
            m_info->destroy(at(last));
        }
        --m_count;
    }

private:
    void swap(Column& o) noexcept {
        std::swap(m_info, o.m_info);
        std::swap(m_data, o.m_data);
        std::swap(m_count, o.m_count);
        std::swap(m_capacity, o.m_capacity);
    }

    void reserve(std::size_t needed) {
        if (needed <= m_capacity) return;
        std::size_t cap = m_capacity ? m_capacity * 2 : 4;
        if (cap < needed) cap = needed;
        auto* nd = static_cast<std::byte*>(
            ::operator new(cap * m_info->size, std::align_val_t(m_info->align)));
        for (std::size_t i = 0; i < m_count; ++i) {
            m_info->moveConstruct(nd + i * m_info->size, at(i));
            m_info->destroy(at(i));
        }
        release();
        m_data = nd;
        m_capacity = cap;
    }

    void destroyAll() {
        if (!m_info || !m_data) return;
        for (std::size_t i = 0; i < m_count; ++i) m_info->destroy(at(i));
        m_count = 0;
    }

    void release() {
        if (m_data) {
            ::operator delete(m_data, std::align_val_t(m_info->align));
            m_data = nullptr;
            m_capacity = 0;
        }
    }

    const ComponentInfo* m_info{nullptr};
    std::byte* m_data{nullptr};
    std::size_t m_count{0};
    std::size_t m_capacity{0};
};

/**
 * @brief A group of entities sharing the same (sorted) component signature.
 *
 * Archetype instances are owned by the Registry via unique_ptr so their address
 * is stable; the Registry stores a raw Archetype* per entity record.
 */
struct Archetype {
    std::vector<ComponentId> signature;   ///< Sorted component ids (unique key).
    std::vector<Column> columns;          ///< One column per signature entry.
    std::vector<std::uint32_t> entities;  ///< Entity slot index per row.

    /// Index of the column for @p id, or -1 if this archetype lacks it.
    int columnIndex(ComponentId id) const {
        for (std::size_t i = 0; i < signature.size(); ++i) {
            if (signature[i] == id) return static_cast<int>(i);
        }
        return -1;
    }

    bool has(ComponentId id) const { return columnIndex(id) >= 0; }
    std::size_t rows() const { return entities.size(); }

    /**
     * @brief Swap-remove @p row across all columns and the entity array.
     * @return the entity slot index relocated into @p row, or Entity::kInvalidIndex
     *         if @p row was the last row (nothing relocated).
     *
     * The caller is responsible for fixing up the relocated entity's record.
     */
    std::uint32_t removeRow(std::size_t row) {
        const std::size_t last = entities.size() - 1;
        for (auto& column : columns) column.removeSwap(row);
        std::uint32_t relocated = 0xFFFFFFFFu;
        if (row != last) {
            entities[row] = entities[last];
            relocated = entities[row];
        }
        entities.pop_back();
        return relocated;
    }
};

} // namespace ecs
} // namespace IKore
