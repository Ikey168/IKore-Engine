#pragma once

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

/**
 * @file Desync.h
 * @brief Desync detection, diagnostics, and input-log replay (issue #161).
 *
 * Milestone 14. Tooling on top of the deterministic core (#159) and rollback
 * netcode (#160) to find and diagnose desyncs and to reproduce a session from a
 * recorded input log:
 *
 *   - ChecksumTrace: the per-tick state checksums (from StateHash) that peers
 *     exchange. firstDivergence() reports the first tick whose checksum differs,
 *     with both hashes - the cheap, network-exchangeable detector.
 *   - diagnoseStates(): given the full per-tick state on each side (local debug
 *     capture), find the first diverging tick and a caller-supplied description of
 *     what differs - the actionable "tick + diverging state" diagnostic.
 *   - InputLog + replay(): record every player's input per tick (optionally
 *     serialized to text); replay feeds the log back through the deterministic
 *     step to reproduce the exact checksum trace, so a recorded log reproduces a
 *     session deterministically.
 *
 * Header-only and dependency-free; templated on the user's State / Input so it
 * works with any deterministic simulation.
 */
namespace IKore {
namespace net {

/// Per-tick state checksums (e.g. StateHash digests). Peers compare these.
struct ChecksumTrace {
    std::vector<std::uint64_t> hashes;

    void record(std::uint64_t hash) { hashes.push_back(hash); }
    std::size_t size() const { return hashes.size(); }
    std::uint64_t at(std::size_t tick) const { return hashes[tick]; }
};

/// The result of a desync check: where (and how) two traces diverged.
struct DesyncReport {
    bool desynced{false};
    int tick{-1};               ///< first diverging tick, or -1 if in sync.
    std::uint64_t localHash{0};
    std::uint64_t remoteHash{0};
    std::string detail;         ///< human-readable description of the divergence.

    std::string summary() const {
        if (!desynced) return "in sync";
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "desync at tick %d: local=0x%016llx remote=0x%016llx", tick,
                      static_cast<unsigned long long>(localHash),
                      static_cast<unsigned long long>(remoteHash));
        std::string s = buf;
        if (!detail.empty()) s += " | " + detail;
        return s;
    }
};

/**
 * @brief First tick at which two checksum traces differ.
 *
 * Reports a checksum mismatch at the earliest differing tick, or - if one peer
 * simply ran fewer ticks - a length mismatch at the shorter length.
 */
inline DesyncReport firstDivergence(const ChecksumTrace& local, const ChecksumTrace& remote) {
    DesyncReport r;
    const std::size_t n = local.size() < remote.size() ? local.size() : remote.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (local.at(i) != remote.at(i)) {
            r.desynced = true;
            r.tick = static_cast<int>(i);
            r.localHash = local.at(i);
            r.remoteHash = remote.at(i);
            r.detail = "checksum mismatch";
            return r;
        }
    }
    if (local.size() != remote.size()) {
        r.desynced = true;
        r.tick = static_cast<int>(n);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "trace length mismatch (local %zu vs remote %zu)",
                      local.size(), remote.size());
        r.detail = buf;
    }
    return r;
}

/**
 * @brief First tick whose state differs, with a caller-supplied description.
 *
 * @p diff is called as diff(tick, localState, remoteState) and returns a non-empty
 * string describing the difference (empty if the two states are identical). This
 * turns a bare checksum mismatch into an actionable "tick + diverging fields"
 * report for local debugging.
 */
template <class State, class DiffFn>
DesyncReport diagnoseStates(const std::vector<State>& local, const std::vector<State>& remote,
                            DiffFn diff) {
    DesyncReport r;
    const std::size_t n = local.size() < remote.size() ? local.size() : remote.size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::string d = diff(static_cast<int>(i), local[i], remote[i]);
        if (!d.empty()) {
            r.desynced = true;
            r.tick = static_cast<int>(i);
            r.detail = d;
            return r;
        }
    }
    if (local.size() != remote.size()) {
        r.desynced = true;
        r.tick = static_cast<int>(n);
        r.detail = "trace length mismatch";
    }
    return r;
}

/// A recorded log of every player's input per tick (a reproducible session).
template <class Input>
struct InputLog {
    std::uint64_t seed{0};
    int numPlayers{0};
    std::vector<std::vector<Input>> ticks; ///< ticks[t][player].

    void record(const std::vector<Input>& perPlayer) { ticks.push_back(perPlayer); }
    std::size_t size() const { return ticks.size(); }
};

/**
 * @brief Replay a recorded input log through the deterministic step, returning the
 *        per-tick checksum trace. Identical inputs reproduce identical state.
 *
 * @p step advances State by one tick: step(State&, inputsForTick, tick).
 * @p hash maps a State to its checksum (e.g. via StateHash).
 */
template <class State, class Input, class StepFn, class HashFn>
ChecksumTrace replay(const InputLog<Input>& log, State state, StepFn step, HashFn hash) {
    ChecksumTrace trace;
    for (std::size_t t = 0; t < log.ticks.size(); ++t) {
        step(state, log.ticks[t], static_cast<int>(t));
        trace.record(hash(state));
    }
    return trace;
}

/// Serialize a log to text (header line + one line of encoded inputs per tick).
/// @p encode maps an Input to a whitespace-free token.
template <class Input, class EncodeFn>
std::string serializeLog(const InputLog<Input>& log, EncodeFn encode) {
    std::ostringstream out;
    out << log.seed << ' ' << log.numPlayers << ' ' << log.ticks.size() << '\n';
    for (const std::vector<Input>& tick : log.ticks) {
        for (std::size_t p = 0; p < tick.size(); ++p) {
            if (p) out << ' ';
            out << encode(tick[p]);
        }
        out << '\n';
    }
    return out.str();
}

/// Parse a log produced by serializeLog. @p decode maps a token back to an Input.
template <class Input, class DecodeFn>
InputLog<Input> parseLog(const std::string& text, DecodeFn decode) {
    InputLog<Input> log;
    std::istringstream in(text);
    std::size_t numTicks = 0;
    in >> log.seed >> log.numPlayers >> numTicks;
    for (std::size_t t = 0; t < numTicks; ++t) {
        std::vector<Input> tick;
        for (int p = 0; p < log.numPlayers; ++p) {
            std::string token;
            in >> token;
            tick.push_back(decode(token));
        }
        log.ticks.push_back(std::move(tick));
    }
    return log;
}

} // namespace net
} // namespace IKore
