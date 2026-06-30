#pragma once

#include <cstdint>

/**
 * @file Fixed.h
 * @brief Deterministic fixed-point real number for lockstep netcode (issue #159).
 *
 * Milestone 14. Floating point is the main cross-platform non-determinism hazard:
 * x87 vs SSE, fused multiply-add contraction, compiler fast-math, and libm
 * transcendentals all produce subtly different results on different machines and
 * builds, which desyncs a lockstep simulation. Fixed replaces float in the
 * simulation core with a Q16.16 fixed-point value stored in an int32_t: every
 * operation is pure integer arithmetic with 64-bit intermediates, so the result
 * is bit-identical on every platform and compiler.
 *
 * 16 fractional bits give a resolution of 1/65536 (~1.5e-5) over a range of about
 * +/-32768 - suitable for bounded simulation coordinates. Construct canonical
 * inputs without float via fromInt / fromFraction / fromRaw so that all machines
 * quantize identical inputs identically; fromFloat exists for convenience but
 * should only be used for build-time constants. Arithmetic saturates rather than
 * overflowing, and division truncates toward zero - both deterministic.
 *
 * Header-only and dependency-free.
 */
namespace IKore {
namespace sim {

class Fixed {
public:
    static constexpr int kFracBits = 16;
    static constexpr int32_t kOneRaw = 1 << kFracBits;

    constexpr Fixed() = default;

    /// The underlying Q16.16 integer (this is the canonical, hashable state).
    int32_t raw{0};

    static constexpr Fixed fromRaw(int32_t r) {
        Fixed f;
        f.raw = r;
        return f;
    }
    static Fixed fromInt(int n) { return fromRaw(narrow(static_cast<int64_t>(n) * kOneRaw)); }
    /// Exact rational num/den without float (deterministic quantization of inputs).
    static Fixed fromFraction(int num, int den) {
        if (den == 0) return fromRaw(num < 0 ? INT32_MIN : INT32_MAX);
        return fromRaw(narrow((static_cast<int64_t>(num) * kOneRaw) / den));
    }
    /// Convenience for build-time constants only; avoid on the simulation hot path.
    static Fixed fromFloat(double d) {
        return fromRaw(narrow(static_cast<int64_t>(d * static_cast<double>(kOneRaw))));
    }

    static constexpr Fixed zero() { return fromRaw(0); }
    static constexpr Fixed one() { return fromRaw(kOneRaw); }

    int toInt() const { return static_cast<int>(raw >> kFracBits); } // floor
    double toDouble() const { return static_cast<double>(raw) / static_cast<double>(kOneRaw); }

    Fixed operator+(Fixed o) const { return fromRaw(narrow(static_cast<int64_t>(raw) + o.raw)); }
    Fixed operator-(Fixed o) const { return fromRaw(narrow(static_cast<int64_t>(raw) - o.raw)); }
    Fixed operator-() const { return fromRaw(narrow(-static_cast<int64_t>(raw))); }

    Fixed operator*(Fixed o) const {
        const int64_t product = static_cast<int64_t>(raw) * o.raw;
        return fromRaw(narrow(product >> kFracBits)); // arithmetic shift = /2^16
    }
    Fixed operator/(Fixed o) const {
        if (o.raw == 0) return fromRaw(raw < 0 ? INT32_MIN : INT32_MAX); // saturate
        const int64_t numerator = static_cast<int64_t>(raw) * kOneRaw;
        return fromRaw(narrow(numerator / o.raw)); // truncates toward zero
    }

    Fixed& operator+=(Fixed o) { return *this = *this + o; }
    Fixed& operator-=(Fixed o) { return *this = *this - o; }
    Fixed& operator*=(Fixed o) { return *this = *this * o; }
    Fixed& operator/=(Fixed o) { return *this = *this / o; }

    bool operator==(Fixed o) const { return raw == o.raw; }
    bool operator!=(Fixed o) const { return raw != o.raw; }
    bool operator<(Fixed o) const { return raw < o.raw; }
    bool operator<=(Fixed o) const { return raw <= o.raw; }
    bool operator>(Fixed o) const { return raw > o.raw; }
    bool operator>=(Fixed o) const { return raw >= o.raw; }

    Fixed abs() const { return fromRaw(raw < 0 ? narrow(-static_cast<int64_t>(raw)) : raw); }

    /// Deterministic square root (integer Newton-free bit method; no float).
    static Fixed sqrt(Fixed x) {
        if (x.raw <= 0) return zero();
        // sqrt(x) in Q16.16: result_raw = isqrt(raw << 16).
        const uint64_t r = isqrt(static_cast<uint64_t>(x.raw) << kFracBits);
        return fromRaw(narrow(static_cast<int64_t>(r)));
    }

    /// Integer square root of a 64-bit value (floor), pure integer math.
    static uint64_t isqrt(uint64_t n) {
        uint64_t result = 0;
        uint64_t bit = static_cast<uint64_t>(1) << 62;
        while (bit > n) bit >>= 2;
        while (bit != 0) {
            if (n >= result + bit) {
                n -= result + bit;
                result = (result >> 1) + bit;
            } else {
                result >>= 1;
            }
            bit >>= 2;
        }
        return result;
    }

private:
    /// Saturating narrow from the 64-bit intermediate to the Q16.16 int32 (deterministic).
    static constexpr int32_t narrow(int64_t v) {
        return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : static_cast<int32_t>(v));
    }
};

} // namespace sim
} // namespace IKore
