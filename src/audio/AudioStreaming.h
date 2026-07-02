#pragma once

#include <cmath>
#include <cstdint>

/**
 * @file AudioStreaming.h
 * @brief Cross-fade and streaming buffer-queue logic for audio (issue #258).
 *
 * The renderer-agnostic, unit-testable logic behind two audio behaviors that were
 * previously no-ops:
 *
 *   - Crossfade: time-based gains for fading one source out while another fades in,
 *     so ambient-zone changes cross-fade instead of hard-swapping. Supports an
 *     equal-power curve (constant perceived loudness through the blend) or a linear
 *     one.
 *   - StreamQueue: the buffer queue/unqueue state machine an OpenAL streaming source
 *     drives - prime a ring of buffers, then each tick unqueue the processed ones,
 *     pull fresh audio from a decoder, and requeue. Modeling the decoder as a pull
 *     callback keeps the logic testable without a device or OpenAL.
 *
 * Header-only, std only, no wall clock (time is passed in as a delta).
 */
namespace IKore {
namespace audio {

/// Per-source gains during a cross-fade: the outgoing source and the incoming one.
struct CrossfadeGains {
    float fromGain{0.0f};
    float toGain{1.0f};
};

/// A time-driven cross-fade from one source (fromGain) to another (toGain).
class Crossfade {
public:
    explicit Crossfade(float durationSeconds = 0.5f, bool equalPower = true)
        : m_duration(durationSeconds > 0.0f ? durationSeconds : 0.0f), m_equalPower(equalPower) {}

    /// Restart the fade with a new duration (0 = instant).
    void reset(float durationSeconds) {
        m_duration = durationSeconds > 0.0f ? durationSeconds : 0.0f;
        m_elapsed = 0.0f;
    }

    void advance(float dt) {
        if (dt > 0.0f) m_elapsed += dt;
        if (m_elapsed > m_duration) m_elapsed = m_duration;
    }

    /// Fade progress in [0, 1]; 1 immediately when the duration is 0.
    float progress() const { return m_duration > 0.0f ? m_elapsed / m_duration : 1.0f; }
    bool done() const { return progress() >= 1.0f; }

    CrossfadeGains gains() const {
        const float t = progress();
        if (m_equalPower) {
            const float halfPi = 1.57079632679489661923f;
            return CrossfadeGains{std::cos(t * halfPi), std::sin(t * halfPi)};
        }
        return CrossfadeGains{1.0f - t, t};
    }

private:
    float m_duration;
    float m_elapsed{0.0f};
    bool m_equalPower;
};

/// One tick's queue activity.
struct StreamStep {
    int unqueued{0};
    int requeued{0};
    bool endOfStream{false};
};

/**
 * @brief The buffer queue/unqueue state machine for a streaming source.
 *
 * Holds a fixed ring of @p bufferCount buffers. prime() fills them from the decoder;
 * update() is called each tick with how many buffers the source reported as played
 * (processed), unqueues those, and refills+requeues them from the decoder while it
 * still yields audio. The decoder is a pull callback returning the number of frames
 * it wrote into a buffer (0 = end of stream). Looping is the callback's job (rewind
 * and keep returning > 0).
 */
class StreamQueue {
public:
    explicit StreamQueue(int bufferCount) : m_bufferCount(bufferCount > 0 ? bufferCount : 1) {}

    /// Fill all buffers initially. Returns how many were queued (fewer if the stream
    /// is shorter than the ring). @p pull returns frames written (0 = end).
    template <class PullFn>
    int prime(PullFn&& pull) {
        m_queued = 0;
        m_ended = false;
        m_framesStreamed = 0;
        for (int i = 0; i < m_bufferCount; ++i) {
            const long long frames = pull();
            if (frames > 0) {
                ++m_queued;
                m_framesStreamed += frames;
            } else {
                m_ended = true;
                break;
            }
        }
        return m_queued;
    }

    /// Recycle @p processedCount played buffers: unqueue each, and requeue it if the
    /// decoder still has audio. Returns the tick's activity.
    template <class PullFn>
    StreamStep update(int processedCount, PullFn&& pull) {
        StreamStep step;
        if (processedCount > m_queued) processedCount = m_queued;
        for (int i = 0; i < processedCount; ++i) {
            --m_queued;
            ++step.unqueued;
            if (m_ended) continue;
            const long long frames = pull();
            if (frames > 0) {
                ++m_queued;
                ++step.requeued;
                m_framesStreamed += frames;
            } else {
                m_ended = true;
            }
        }
        step.endOfStream = m_ended;
        return step;
    }

    int bufferCount() const { return m_bufferCount; }
    int queuedCount() const { return m_queued; }
    /// The stream is finished once the decoder ended and every buffer has drained.
    bool finished() const { return m_ended && m_queued == 0; }
    long long framesStreamed() const { return m_framesStreamed; }

private:
    int m_bufferCount;
    int m_queued{0};
    bool m_ended{false};
    long long m_framesStreamed{0};
};

} // namespace audio
} // namespace IKore
