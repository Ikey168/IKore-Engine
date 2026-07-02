// Test for the particle emitter cadence + live-count estimate - issue #267.
//
// Moving emission onto the GPU must not change emitter behavior (rate, bursts,
// looping) from the user's perspective, and rendering straight from the SSBO removes
// the per-frame readback that used to give an exact live count. These helpers are the
// scalar bookkeeping both paths share; testing them here locks the cadence and the
// readback-free count estimate (the GPU path itself cannot run in this environment).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_particle_emit.cpp -o test_particle_emit

#include "render/ParticleEmit.h"

#include <cmath>
#include <cstdio>

using namespace IKore::render;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// A fractional rate emits at the correct average cadence via the carried remainder.
static void testFractionalCadence() {
    float remainder = 0.0f;
    // 0.4 particles/step: expect 0,0,1,0,1 pattern (cumulative floor).
    int total = 0;
    int emittedPerStep[5];
    for (int i = 0; i < 5; ++i) {
        emittedPerStep[i] = particlesToEmit(4.0f, 0.1f, remainder); // 4/s * 0.1s = 0.4
        total += emittedPerStep[i];
    }
    CHECK(emittedPerStep[0] == 0);
    CHECK(emittedPerStep[1] == 0);
    CHECK(emittedPerStep[2] == 1);
    CHECK(emittedPerStep[3] == 0);
    CHECK(emittedPerStep[4] == 1);
    CHECK(total == 2); // 5 * 0.4 = 2.0

    // Over many steps the count tracks rate*time closely (no drift from the carry).
    remainder = 0.0f;
    total = 0;
    for (int i = 0; i < 600; ++i) total += particlesToEmit(50.0f, 1.0f / 60.0f, remainder);
    CHECK(total == 500); // 50/s over 10s
}

// A whole rate emits exactly that many each step; zero/negative rate emits nothing.
static void testWholeAndZeroRate() {
    float remainder = 0.0f;
    CHECK(particlesToEmit(100.0f, 0.01f, remainder) == 1); // 1.0 exactly
    remainder = 0.0f;
    CHECK(particlesToEmit(0.0f, 0.5f, remainder) == 0);
    remainder = 0.0f;
    CHECK(particlesToEmit(-10.0f, 0.5f, remainder) == 0); // negative clamps to 0
}

static void testLoopingGuard() {
    // Looping emitters never finish.
    CHECK(!emissionFinished(true, 100.0f, 2.0f));
    // Non-looping: finished once the run exceeds startLife.
    CHECK(!emissionFinished(false, 1.9f, 2.0f));
    CHECK(!emissionFinished(false, 2.0f, 2.0f)); // strictly greater
    CHECK(emissionFinished(false, 2.1f, 2.0f));
}

// The readback-free estimate converges to emissionRate * startLife at steady state.
static void testActiveEstimateConverges() {
    const float rate = 50.0f;      // particles/sec
    const float startLife = 3.0f;  // sec
    const float dt = 1.0f / 60.0f;
    const int maxParticles = 1000;
    float remainder = 0.0f;
    float active = 0.0f;
    for (int i = 0; i < 6000; ++i) { // 100 seconds, plenty to settle
        const int emitted = particlesToEmit(rate, dt, remainder);
        active = estimateActiveParticles(active, emitted, dt, startLife, maxParticles);
    }
    const float expected = rate * startLife; // 150
    CHECK(std::fabs(active - expected) < expected * 0.1f);
    CHECK(active <= static_cast<float>(maxParticles));
}

// The estimate never exceeds capacity and decays to (effectively) zero.
static void testActiveEstimateClamped() {
    // Huge emission clamps to capacity.
    CHECK(estimateActiveParticles(0.0f, 100000, 0.016f, 3.0f, 500) == 500.0f);
    // No emission, everything decays toward 0.
    float active = 500.0f;
    for (int i = 0; i < 100000; ++i) active = estimateActiveParticles(active, 0, 0.5f, 3.0f, 500);
    CHECK(active < 0.5f); // rounds to 0 particles
    // startLife <= 0 does not divide by zero (no death term applied).
    CHECK(estimateActiveParticles(10.0f, 5, 0.016f, 0.0f, 500) == 15.0f);
}

int main() {
    testFractionalCadence();
    testWholeAndZeroRate();
    testLoopingGuard();
    testActiveEstimateConverges();
    testActiveEstimateClamped();

    if (g_failures == 0) {
        std::printf("All particle emit tests passed.\n");
        return 0;
    }
    std::printf("%d particle emit test(s) failed.\n", g_failures);
    return 1;
}
