#pragma once

#include <cmath>
#include <cstdint>

/**
 * @file Simulation.h
 * @brief Deterministic fixed-step simulation loop (issue #152).
 *
 * Milestone 12 foundation. Provides:
 *   - FixedTimestep: the "fix your timestep" accumulator. A variable-length
 *     render frame is consumed into a whole number of fixed-size simulation
 *     steps, with a leftover used as an interpolation `alpha` - so rendering is
 *     decoupled from the simulation rate and the sim advances in reproducible
 *     fixed increments.
 *   - DeterministicRng: a fully-specified SplitMix64 PRNG (portable integer math)
 *     so the same seed yields the same sequence on every platform.
 *   - Simulation: ties them together with a tick counter and an owned RNG, so the
 *     same seed + the same frame-delta inputs produce identical state across runs.
 *
 * Header-only and dependency-free, so it is unit-tested in isolation; this is the
 * substrate the agent simulation (#153-#155) builds on.
 */
namespace IKore {
namespace sim {

/// SplitMix64 - small, fast, fully-specified, portable deterministic PRNG.
class DeterministicRng {
public:
    explicit DeterministicRng(std::uint64_t seedValue = 0) : m_state(seedValue) {}

    void seed(std::uint64_t seedValue) { m_state = seedValue; }

    std::uint64_t nextU64() {
        std::uint64_t z = (m_state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    /// Uniform double in [0, 1).
    double nextDouble() { return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0); }
    float nextFloat() { return static_cast<float>(nextDouble()); }

    /// Uniform integer in [lo, hi].
    int nextInt(int lo, int hi) {
        if (hi <= lo) return lo;
        const std::uint64_t range = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(nextU64() % range);
    }

private:
    std::uint64_t m_state;
};

/// Accumulator that turns a variable frame delta into fixed simulation steps.
struct FixedTimestep {
    double fixedDelta{1.0 / 60.0}; ///< seconds per simulation step
    double accumulator{0.0};
    int maxSteps{8}; ///< cap per frame to avoid the "spiral of death"

    /// Accumulate @p frameDelta and return how many fixed steps to run now.
    int stepsFor(double frameDelta) {
        if (fixedDelta <= 0.0) return 0;
        accumulator += frameDelta;
        int n = 0;
        while (accumulator >= fixedDelta && n < maxSteps) {
            accumulator -= fixedDelta;
            ++n;
        }
        if (accumulator >= fixedDelta) {
            // Hit the cap: drop the excess whole steps (keep only the remainder)
            // so a long stall doesn't snowball into unbounded catch-up.
            accumulator = std::fmod(accumulator, fixedDelta);
        }
        return n;
    }

    /// Interpolation factor in [0, 1) for rendering between fixed steps.
    double alpha() const { return fixedDelta > 0.0 ? accumulator / fixedDelta : 0.0; }
};

class Simulation {
public:
    explicit Simulation(std::uint64_t seed = 0, double fixedDelta = 1.0 / 60.0, int maxSteps = 8)
        : m_rng(seed) {
        m_timestep.fixedDelta = fixedDelta;
        m_timestep.maxSteps = maxSteps;
    }

    /// Re-seed the RNG and clear tick/accumulator (for repeatable runs).
    void reset(std::uint64_t seed) {
        m_rng.seed(seed);
        m_tick = 0;
        m_timestep.accumulator = 0.0;
    }

    /**
     * @brief Advance the simulation by a render frame of @p frameDelta seconds.
     *        Runs zero or more fixed steps; @p step is called once per fixed step
     *        as step(tick, rng). Returns the number of steps run.
     */
    template <class StepFn>
    int advance(double frameDelta, StepFn&& step) {
        const int steps = m_timestep.stepsFor(frameDelta);
        for (int i = 0; i < steps; ++i) {
            step(m_tick, m_rng);
            ++m_tick;
        }
        return steps;
    }

    std::uint64_t tick() const { return m_tick; }
    double fixedDelta() const { return m_timestep.fixedDelta; }
    double alpha() const { return m_timestep.alpha(); }
    DeterministicRng& rng() { return m_rng; }

private:
    FixedTimestep m_timestep;
    DeterministicRng m_rng;
    std::uint64_t m_tick{0};
};

} // namespace sim
} // namespace IKore
