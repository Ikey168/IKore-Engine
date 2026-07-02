// Test for the audio cross-fade + streaming buffer-queue logic - issue #258.
//
// Verifies the equal-power/linear cross-fade curves (used for ambient-zone
// transitions instead of a hard swap) and the StreamQueue buffer queue/unqueue
// state machine an OpenAL streaming source drives. A fake pull-callback decoder
// stands in for the audio source, so the logic is checked without a device.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_audio_streaming.cpp -o test_audio_streaming

#include "audio/AudioStreaming.h"

#include <cmath>
#include <cstdio>

using namespace IKore::audio;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

static void testCrossfadeEqualPower() {
    Crossfade cf(1.0f, /*equalPower=*/true);
    // Start: outgoing full, incoming silent.
    CHECK(approx(cf.gains().fromGain, 1.0f));
    CHECK(approx(cf.gains().toGain, 0.0f));
    CHECK(!cf.done());

    // Midpoint: equal gains, constant power (sum of squares ~ 1).
    cf.advance(0.5f);
    const CrossfadeGains mid = cf.gains();
    CHECK(approx(mid.fromGain, mid.toGain));
    CHECK(approx(mid.fromGain * mid.fromGain + mid.toGain * mid.toGain, 1.0f, 2e-3f));

    // End: fully faded to the incoming source.
    cf.advance(0.5f);
    CHECK(cf.done());
    CHECK(approx(cf.gains().fromGain, 0.0f));
    CHECK(approx(cf.gains().toGain, 1.0f));

    // advance past the end clamps (progress stays 1).
    cf.advance(5.0f);
    CHECK(approx(cf.progress(), 1.0f));
}

static void testCrossfadeLinearResetAndInstant() {
    Crossfade cf(2.0f, /*equalPower=*/false);
    cf.advance(1.0f); // halfway
    CHECK(approx(cf.gains().fromGain, 0.5f));
    CHECK(approx(cf.gains().toGain, 0.5f));

    // reset restarts the fade.
    cf.reset(1.0f);
    CHECK(approx(cf.gains().fromGain, 1.0f));
    CHECK(!cf.done());

    // Zero-duration fade is instant.
    Crossfade instant(0.0f, false);
    CHECK(instant.done());
    CHECK(approx(instant.gains().toGain, 1.0f));
}

static void testStreamQueueDrains() {
    // Decoder yields 5 chunks of 10 frames, then end of stream.
    int chunksLeft = 5;
    auto pull = [&]() -> long long { return chunksLeft-- > 0 ? 10 : 0; };

    StreamQueue q(3);
    CHECK(q.prime(pull) == 3);      // ring filled
    CHECK(q.queuedCount() == 3);
    CHECK(!q.finished());
    CHECK(q.framesStreamed() == 30); // 3 primed chunks

    // Two ticks recycle one buffer each and refill from the decoder (chunks 4, 5).
    StreamStep s1 = q.update(1, pull);
    CHECK(s1.unqueued == 1 && s1.requeued == 1);
    StreamStep s2 = q.update(1, pull);
    CHECK(s2.requeued == 1);
    CHECK(q.framesStreamed() == 50); // all 5 chunks pulled
    CHECK(q.queuedCount() == 3);

    // Decoder now exhausted: further ticks unqueue without requeue until it drains.
    StreamStep s3 = q.update(1, pull);
    CHECK(s3.unqueued == 1 && s3.requeued == 0 && s3.endOfStream);
    CHECK(q.queuedCount() == 2);
    q.update(2, pull);
    CHECK(q.queuedCount() == 0);
    CHECK(q.finished());
}

static void testStreamQueueShortAndLoop() {
    // Stream shorter than the ring: only 2 chunks available for a 4-buffer ring.
    int left = 2;
    auto shortPull = [&]() -> long long { return left-- > 0 ? 8 : 0; };
    StreamQueue s(4);
    CHECK(s.prime(shortPull) == 2); // only 2 filled
    CHECK(!s.finished());           // the 2 buffers still need to drain
    s.update(2, shortPull);
    CHECK(s.finished());

    // Looping decoder never returns 0: the queue never finishes and keeps streaming.
    auto loopPull = []() -> long long { return 16; };
    StreamQueue l(3);
    l.prime(loopPull);
    for (int i = 0; i < 100; ++i) l.update(2, loopPull);
    CHECK(!l.finished());
    CHECK(l.queuedCount() == 3);
    CHECK(l.framesStreamed() > 1000);

    // processedCount is clamped to what is queued.
    StreamQueue c(2);
    c.prime(loopPull);
    StreamStep step = c.update(99, loopPull);
    CHECK(step.unqueued == 2); // only 2 were queued
}

int main() {
    testCrossfadeEqualPower();
    testCrossfadeLinearResetAndInstant();
    testStreamQueueDrains();
    testStreamQueueShortAndLoop();

    if (g_failures == 0) {
        std::printf("All audio streaming tests passed.\n");
        return 0;
    }
    std::printf("%d audio streaming test(s) failed.\n", g_failures);
    return 1;
}
