#pragma once

#include "core/sim/Fixed.h"

#include <cstdint>

/**
 * @file StateHash.h
 * @brief Deterministic checksum of simulation state for lockstep (issue #159).
 *
 * Milestone 14. In lockstep / rollback netcode each peer advances the same
 * deterministic simulation from the same inputs and periodically exchanges a
 * checksum of its state; a mismatch flags a desync. StateHash is a 64-bit FNV-1a
 * accumulator fed the canonical integer representation of state (Fixed::raw, RNG
 * state, tick counts, entity fields), never floats - so the digest is bit-
 * identical across platforms whenever the state is.
 *
 * The hash is order-dependent by design: peers must feed values in the same order
 * (e.g. iterate entities in the ECS's deterministic archetype/row order, and any
 * std::map in key order - never an unordered_map). Header-only.
 */
namespace IKore {
namespace sim {

class StateHash {
public:
    /// Feed a raw 64-bit value (one FNV-1a step).
    void add(uint64_t value) {
        m_hash ^= value;
        m_hash *= kPrime;
    }
    void addI64(int64_t value) { add(static_cast<uint64_t>(value)); }
    void addU32(uint32_t value) { add(static_cast<uint64_t>(value)); }
    void addI32(int32_t value) { add(static_cast<uint64_t>(static_cast<uint32_t>(value))); }

    /// Feed a Fixed by its exact integer representation (never its float value).
    void addFixed(Fixed value) { addI32(value.raw); }

    uint64_t digest() const { return m_hash; }
    void reset() { m_hash = kOffsetBasis; }

private:
    static constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
    static constexpr uint64_t kPrime = 1099511628211ULL;
    uint64_t m_hash{kOffsetBasis};
};

} // namespace sim
} // namespace IKore
