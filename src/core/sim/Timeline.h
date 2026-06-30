#pragma once

#include "core/sim/Simulation.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>

/**
 * @file Timeline.h
 * @brief Time control (pause / step / speed) and rewind / replay (issue #154).
 *
 * Milestone 12. Wraps the deterministic fixed-step loop (#152) with interactive
 * time controls and a bounded history so a running simulation can be paused,
 * single-stepped, sped up or slowed down, and rewound to an earlier tick.
 *
 * Rewind/replay is snapshot based. The Timeline keeps a ring buffer of per-tick
 * snapshots, each capturing the full deterministic state at a tick boundary: the
 * tick number, the RNG's internal state, and a copy of the user @p State. Because
 * the step function is deterministic given (state, tick, rng), restoring a
 * snapshot and stepping forward again reproduces exactly the original trajectory
 * - the issue's reproducibility guarantee. Snapshotting the RNG's internal state
 * (not just the seed) is what makes replay bit-identical.
 *
 * @tparam State the simulation world, a value type that is copyable and cheap
 *         enough to snapshot each tick (e.g. a serializable POD or a small SoA
 *         container). The Registry itself is non-copyable, so callers snapshot a
 *         serializable view of their world here ("building on serialization").
 *
 * Header-only and dependency-free (std + the #152 sim primitives), so it is
 * unit-tested in isolation.
 */
namespace IKore {
namespace sim {

template <class State>
class Timeline {
public:
    /**
     * @param initial         the starting world state (snapshotted as tick 0).
     * @param seed            RNG seed for the deterministic stream.
     * @param fixedDelta      seconds per simulation step.
     * @param historyCapacity max snapshots retained (the rewind window); >= 1.
     * @param maxStepsPerFrame spiral-of-death cap on real-time-driven steps.
     */
    explicit Timeline(State initial, std::uint64_t seed = 0, double fixedDelta = 1.0 / 60.0,
                      std::size_t historyCapacity = 256, int maxStepsPerFrame = 8)
        : m_state(std::move(initial)),
          m_rng(seed),
          m_capacity(historyCapacity == 0 ? 1 : historyCapacity) {
        m_timestep.fixedDelta = fixedDelta;
        m_timestep.maxSteps = maxStepsPerFrame;
        m_history.push_back(Snapshot{m_tick, m_rng.state(), m_state}); // S(0)
    }

    // --- Time controls -----------------------------------------------------

    void pause() { m_paused = true; }
    void resume() { m_paused = false; }
    bool paused() const { return m_paused; }

    /// Playback rate: 1.0 normal, 2.0 double speed, 0.5 half. Clamped to >= 0.
    void setTimeScale(double scale) { m_timeScale = scale < 0.0 ? 0.0 : scale; }
    double timeScale() const { return m_timeScale; }

    /// Queue @p n fixed steps to run on the next advance regardless of pause or
    /// elapsed time - the "single-step" control (typically used while paused).
    void requestStep(int n = 1) {
        if (n > 0) m_pendingSteps += n;
    }

    /**
     * @brief Advance by a render frame of @p realDt seconds.
     *
     * Runs any queued single-steps first, then - unless paused - consumes
     * @p realDt scaled by the time scale into fixed steps. @p step is called per
     * fixed step as step(State&, tick, DeterministicRng&). Returns steps run.
     */
    template <class StepFn>
    int advance(double realDt, StepFn&& step) {
        int ran = 0;
        while (m_pendingSteps > 0) { // explicit single steps always run
            runStep(step);
            --m_pendingSteps;
            ++ran;
        }
        if (!m_paused) {
            const int steps = m_timestep.stepsFor(realDt * m_timeScale);
            for (int i = 0; i < steps; ++i) {
                runStep(step);
                ++ran;
            }
        }
        return ran;
    }

    // --- Rewind / replay ---------------------------------------------------

    /// Current simulation tick (number of steps run since construction).
    std::uint64_t tick() const { return m_tick; }
    /// Earliest tick still recoverable from the history window.
    std::uint64_t oldestTick() const { return m_history.front().tick; }
    /// History snapshots currently retained.
    std::size_t historySize() const { return m_history.size(); }

    /// True if @p t is within the retained window (oldestTick .. current tick).
    bool canRewindTo(std::uint64_t t) const {
        return t <= m_tick && t >= m_history.front().tick;
    }

    /**
     * @brief Rewind to tick @p t: restore the state and RNG as they were at that
     *        tick and drop the now-invalid future snapshots. Replaying forward
     *        from here reproduces the original trajectory.
     * @return false (no change) if @p t is outside the retained window.
     */
    bool rewindTo(std::uint64_t t) {
        if (!canRewindTo(t)) return false;
        while (m_history.back().tick > t) m_history.pop_back(); // drop the future
        const Snapshot& snap = m_history.back();
        m_state = snap.state;
        m_rng.setState(snap.rngState);
        m_tick = snap.tick;
        m_timestep.accumulator = 0.0; // start the replay cleanly on a tick boundary
        return true;
    }

    // --- Accessors ---------------------------------------------------------

    const State& state() const { return m_state; }
    DeterministicRng& rng() { return m_rng; }
    double fixedDelta() const { return m_timestep.fixedDelta; }
    double alpha() const { return m_timestep.alpha(); }

private:
    struct Snapshot {
        std::uint64_t tick;
        std::uint64_t rngState;
        State state;
    };

    template <class StepFn>
    void runStep(StepFn&& step) {
        step(m_state, m_tick, m_rng);
        ++m_tick;
        m_history.push_back(Snapshot{m_tick, m_rng.state(), m_state}); // S(m_tick)
        while (m_history.size() > m_capacity) m_history.pop_front();    // ring eviction
    }

    State m_state;
    DeterministicRng m_rng;
    FixedTimestep m_timestep;
    std::deque<Snapshot> m_history; // bounded ring of per-tick snapshots
    std::size_t m_capacity;
    std::uint64_t m_tick{0};
    int m_pendingSteps{0};
    double m_timeScale{1.0};
    bool m_paused{false};
};

} // namespace sim
} // namespace IKore
