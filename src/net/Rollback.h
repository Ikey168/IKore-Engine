#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

/**
 * @file Rollback.h
 * @brief GGPO-style rollback + input prediction netcode (issue #160).
 *
 * Milestone 14. A deterministic rollback session built on the simulation core
 * (#159): every peer runs the same deterministic step from the same inputs, so
 * given identical inputs they reach identical state. Because remote inputs arrive
 * late (latency) or out of order / dropped (loss), each peer predicts the missing
 * inputs (repeat the last known input) and advances anyway. When an authoritative
 * remote input arrives for a past frame and differs from what was predicted, the
 * session rolls back to that frame, restores the saved state, and resimulates
 * forward with the corrected inputs - producing the correct state with no visible
 * corruption (only a brief silent correction).
 *
 * RollbackSession is templated on the user's State (a value type, snapshotted per
 * frame) and Input (a small value type with operator==). The step function
 * advances State by one fixed frame given every player's input for that frame:
 * use the deterministic Fixed math + DeterministicRng from #159 inside it. This
 * is the engine-agnostic core a NetworkComponent / NetworkSystem can drive.
 *
 * Header-only and dependency-free.
 */
namespace IKore {
namespace net {

template <class Input>
struct InputSlot {
    Input value{};
    bool present{false};   ///< an input (predicted or authoritative) exists here.
    bool confirmed{false}; ///< the input is authoritative (not a prediction).
};

template <class State, class Input>
class RollbackSession {
public:
    /// step(state, inputsForFrame, frame): advance one fixed frame deterministically.
    using StepFn = std::function<void(State&, const std::vector<Input>&, int)>;

    RollbackSession(int numPlayers, int localPlayer, State initial, StepFn step,
                    int maxRollbackFrames = 256)
        : m_numPlayers(numPlayers),
          m_local(localPlayer),
          m_state(std::move(initial)),
          m_step(std::move(step)),
          m_window(maxRollbackFrames < 1 ? 1 : maxRollbackFrames) {
        m_inputs.resize(static_cast<std::size_t>(numPlayers));
        m_snapshots[0] = m_state; // state at frame 0
    }

    int currentFrame() const { return m_frame; }
    const State& state() const { return m_state; }
    int rollbackCount() const { return m_rollbackCount; }
    int numPlayers() const { return m_numPlayers; }

    /**
     * @brief Advance one frame using the local player's authoritative input.
     *        Missing remote inputs are predicted (repeat last known).
     */
    void advanceFrame(const Input& localInput) {
        ensureFrame(m_frame);
        setInput(m_local, m_frame, localInput, /*confirmed=*/true);
        for (int p = 0; p < m_numPlayers; ++p) {
            if (p == m_local) continue;
            if (!slot(p, m_frame).present) {
                setInput(p, m_frame, predict(p, m_frame), /*confirmed=*/false);
            }
        }
        m_snapshots[m_frame] = m_state; // state at this frame, before stepping it
        simulate(m_frame);
        ++m_frame;
        trimSnapshots();
    }

    /**
     * @brief Apply an authoritative input from a remote peer for (player, frame).
     *
     * If that frame was already simulated with a different predicted input, the
     * session rolls back to it and resimulates forward. A future frame is simply
     * recorded and used authoritatively when reached.
     */
    void addRemoteInput(int player, int frame, const Input& input) {
        if (player < 0 || player >= m_numPlayers || frame < 0) return;
        ensureFrame(frame);
        InputSlot<Input>& s = slot(player, frame);
        const bool wasPredicted = s.present && !s.confirmed;
        const bool mispredicted = wasPredicted && !(s.value == input);
        s.value = input;
        s.present = true;
        s.confirmed = true;

        if (frame < m_frame && mispredicted) rollbackAndResimulate(frame);
    }

    /// Highest frame F such that every player's input is confirmed for [0, F).
    int confirmedFrame() const {
        for (int f = 0;; ++f) {
            for (int p = 0; p < m_numPlayers; ++p) {
                const auto& col = m_inputs[static_cast<std::size_t>(p)];
                if (f >= static_cast<int>(col.size()) || !col[static_cast<std::size_t>(f)].present ||
                    !col[static_cast<std::size_t>(f)].confirmed) {
                    return f;
                }
            }
        }
    }

    /// State at a past frame boundary (if still within the snapshot window).
    bool snapshotAt(int frame, State& out) const {
        auto it = m_snapshots.find(frame);
        if (it == m_snapshots.end()) return false;
        out = it->second;
        return true;
    }

private:
    InputSlot<Input>& slot(int p, int frame) {
        return m_inputs[static_cast<std::size_t>(p)][static_cast<std::size_t>(frame)];
    }
    const InputSlot<Input>& slot(int p, int frame) const {
        return m_inputs[static_cast<std::size_t>(p)][static_cast<std::size_t>(frame)];
    }

    void ensureFrame(int frame) {
        for (auto& col : m_inputs) {
            if (static_cast<int>(col.size()) <= frame) col.resize(static_cast<std::size_t>(frame) + 1);
        }
    }

    void setInput(int p, int frame, const Input& value, bool confirmed) {
        InputSlot<Input>& s = slot(p, frame);
        s.value = value;
        s.present = true;
        s.confirmed = confirmed;
    }

    /// Predict a player's input at @p frame: repeat its most recent known input.
    Input predict(int p, int frame) const {
        const auto& col = m_inputs[static_cast<std::size_t>(p)];
        for (int f = frame - 1; f >= 0; --f) {
            if (f < static_cast<int>(col.size()) && col[static_cast<std::size_t>(f)].present) {
                return col[static_cast<std::size_t>(f)].value;
            }
        }
        return Input{};
    }

    void simulate(int frame) {
        std::vector<Input> inputs(static_cast<std::size_t>(m_numPlayers));
        for (int p = 0; p < m_numPlayers; ++p) inputs[static_cast<std::size_t>(p)] = slot(p, frame).value;
        m_step(m_state, inputs, frame);
    }

    void rollbackAndResimulate(int target) {
        auto it = m_snapshots.find(target);
        if (it == m_snapshots.end()) { // too old to correct; clamp to the oldest we kept
            it = m_snapshots.begin();
            target = it->first;
        }
        const int head = m_frame;
        m_state = it->second;
        m_frame = target;
        for (int f = target; f < head; ++f) {
            // Re-predict any still-unconfirmed remote inputs using corrected history.
            for (int p = 0; p < m_numPlayers; ++p) {
                if (p == m_local) continue;
                if (!slot(p, f).confirmed) setInput(p, f, predict(p, f), /*confirmed=*/false);
            }
            m_snapshots[f] = m_state;
            simulate(f);
            ++m_frame;
        }
        ++m_rollbackCount;
    }

    void trimSnapshots() {
        const int oldest = m_frame - m_window;
        if (oldest <= 0) return;
        while (!m_snapshots.empty() && m_snapshots.begin()->first < oldest) {
            m_snapshots.erase(m_snapshots.begin());
        }
    }

    int m_numPlayers;
    int m_local;
    State m_state;
    StepFn m_step;
    int m_window;
    int m_frame{0};
    int m_rollbackCount{0};
    std::vector<std::vector<InputSlot<Input>>> m_inputs; // [player][frame]
    std::map<int, State> m_snapshots;                    // frame -> state at that frame
};

} // namespace net
} // namespace IKore
