// Test for the deterministic simulation core - Milestone 14, #159.
//
// Verifies the issue's acceptance criteria:
//   - Identical inputs produce identical state: a lockstep simulation built on
//     Fixed (fixed-point) + DeterministicRng yields the same state hash across
//     repeat runs and across different step chunking, and matches a pinned golden
//     digest (so a different machine/build that diverged would fail this test).
//   - Sources of non-determinism are controlled: the state is integer Fixed (no
//     float), the only randomness is the integer DeterministicRng, and iteration
//     order is a fixed vector order fed into an order-defined hash.
//
// Pure std + header-only sim core:
//   g++ -std=c++17 -I src tests/test_determinism.cpp -o test_determinism

#include "core/sim/Fixed.h"
#include "core/sim/Simulation.h" // DeterministicRng
#include "core/sim/StateHash.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace IKore::sim;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static void testFixedExactArithmetic() {
    // Exact rationals, no float involved.
    CHECK((Fixed::fromInt(2) + Fixed::fromInt(3)).raw == Fixed::fromInt(5).raw);
    CHECK((Fixed::fromInt(7) - Fixed::fromInt(10)).raw == Fixed::fromInt(-3).raw);

    const Fixed half = Fixed::fromFraction(1, 2);
    const Fixed quarter = Fixed::fromFraction(1, 4);
    CHECK(half.raw == 32768);             // 0.5 in Q16.16
    CHECK(quarter.raw == 16384);          // 0.25
    CHECK((half * half).raw == quarter.raw);   // 0.5 * 0.5 == 0.25 exactly
    CHECK((quarter / half).raw == half.raw);   // 0.25 / 0.5 == 0.5 exactly

    // Square root is exact for perfect squares and deterministic otherwise.
    CHECK(Fixed::sqrt(Fixed::fromInt(4)).raw == Fixed::fromInt(2).raw);
    CHECK(Fixed::sqrt(Fixed::fromInt(9)).raw == Fixed::fromInt(3).raw);
    const Fixed root2 = Fixed::sqrt(Fixed::fromInt(2));
    CHECK(root2.raw == 92681); // floor(sqrt(2) * 65536), a fixed integer on every build

    // Saturation rather than undefined overflow.
    CHECK((Fixed::fromInt(30000) * Fixed::fromInt(30000)).raw == INT32_MAX);
    // Division by zero saturates by sign.
    CHECK((Fixed::fromInt(1) / Fixed::zero()).raw == INT32_MAX);
    CHECK((Fixed::fromInt(-1) / Fixed::zero()).raw == INT32_MIN);
}

// A tiny deterministic lockstep world: integer Fixed agents stepping toward a
// goal, perturbed only by the integer DeterministicRng.
struct Agent {
    Fixed x, z;
};

static void stepWorld(std::vector<Agent>& world, DeterministicRng& rng, Fixed goalX, Fixed goalZ) {
    const Fixed speed = Fixed::fromFraction(1, 10);
    for (Agent& a : world) {
        const Fixed dx = goalX - a.x;
        const Fixed dz = goalZ - a.z;
        const Fixed dist = Fixed::sqrt(dx * dx + dz * dz);
        if (dist.raw > 0) {
            a.x += dx * speed / dist;
            a.z += dz * speed / dist;
        }
        // Deterministic integer perturbation (no float, controlled RNG).
        a.x += Fixed::fromFraction(rng.nextInt(-1, 1), 100);
    }
}

// Run the lockstep sim and return a deterministic hash of the final state.
static uint64_t runSim(std::uint64_t seed, int steps, int agentCount) {
    DeterministicRng rng(seed);
    std::vector<Agent> world(static_cast<std::size_t>(agentCount));
    for (int i = 0; i < agentCount; ++i) {
        world[static_cast<std::size_t>(i)].x = Fixed::fromInt(i);
        world[static_cast<std::size_t>(i)].z = Fixed::fromInt(0);
    }
    const Fixed goalX = Fixed::fromInt(50), goalZ = Fixed::fromInt(50);
    for (int s = 0; s < steps; ++s) stepWorld(world, rng, goalX, goalZ);

    StateHash h;
    for (const Agent& a : world) {
        h.addFixed(a.x);
        h.addFixed(a.z);
    }
    h.addI64(static_cast<int64_t>(rng.state())); // include the RNG stream position
    return h.digest();
}

// Same as runSim but advances in two separate batches, mimicking a peer that
// chunks the steps differently. Must yield the identical hash.
static uint64_t runSimChunked(std::uint64_t seed, int steps, int agentCount) {
    DeterministicRng rng(seed);
    std::vector<Agent> world(static_cast<std::size_t>(agentCount));
    for (int i = 0; i < agentCount; ++i) {
        world[static_cast<std::size_t>(i)].x = Fixed::fromInt(i);
        world[static_cast<std::size_t>(i)].z = Fixed::fromInt(0);
    }
    const Fixed goalX = Fixed::fromInt(50), goalZ = Fixed::fromInt(50);
    const int firstBatch = steps / 3;
    for (int s = 0; s < firstBatch; ++s) stepWorld(world, rng, goalX, goalZ);
    for (int s = firstBatch; s < steps; ++s) stepWorld(world, rng, goalX, goalZ);

    StateHash h;
    for (const Agent& a : world) {
        h.addFixed(a.x);
        h.addFixed(a.z);
    }
    h.addI64(static_cast<int64_t>(rng.state()));
    return h.digest();
}

static void testLockstepDeterminism() {
    // Pinned digest of the lockstep run, captured on a reference build. A platform
    // or build that diverged (float creep, shift semantics, RNG drift) would
    // produce a different digest and fail here.
    const std::uint64_t kGolden = 0x36760d0b0e95a2f0ULL;

    const uint64_t a = runSim(12345, 100, 8);
    const uint64_t b = runSim(12345, 100, 8);

    CHECK(a == b);                          // identical inputs -> identical state
    CHECK(a != runSim(54321, 100, 8));      // different seed -> different state
    CHECK(a == runSimChunked(12345, 100, 8)); // chunking the steps changes nothing
    CHECK(a == kGolden);                    // pinned: a divergent build would fail here
}

int main() {
    testFixedExactArithmetic();
    testLockstepDeterminism();

    if (g_failures == 0) {
        std::printf("All determinism tests passed.\n");
        return 0;
    }
    std::printf("%d determinism test(s) failed.\n", g_failures);
    return 1;
}
