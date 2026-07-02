// Test for the shared per-particle update step - issue #259 (renderer completions).
//
// ParticleSystem::updateCPU and src/shaders/particle_update.compute both mirror
// simulateParticleStep, so locking its semantics here guarantees the CPU and GPU
// particle paths advance particles identically (the prerequisite for the compute
// path being a drop-in). Covers death handling, life decay/clamping, semi-implicit
// Euler order, rotation, and a multi-step reference comparison.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_particle_sim.cpp -o test_particle_sim

#include "render/ParticleSim.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::render;
using IKore::ecs::Vec3;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

static void testDeadParticleUntouched() {
    SimParticle p;
    p.life = 0.0f;
    p.position = Vec3{1, 2, 3};
    p.velocity = Vec3{9, 9, 9};
    CHECK(!simulateParticleStep(p, 0.16f));
    CHECK(approx(p.position.x, 1.0f) && approx(p.position.y, 2.0f) && approx(p.position.z, 3.0f));
}

static void testLifeDecayAndClamp() {
    SimParticle p;
    p.life = 1.0f;
    p.startLife = 2.0f;
    CHECK(simulateParticleStep(p, 0.5f));
    CHECK(approx(p.life, 0.75f)); // 1 - 0.5/2

    // Expiring this step clamps to exactly 0, reports dead, and does not move.
    SimParticle q;
    q.life = 0.1f;
    q.startLife = 1.0f;
    q.velocity = Vec3{5, 0, 0};
    const Vec3 before = q.position;
    CHECK(!simulateParticleStep(q, 0.2f));
    CHECK(q.life == 0.0f);
    CHECK(approx(q.position.x, before.x)); // motion not advanced on the death step
}

static void testSemiImplicitEulerOrder() {
    // Velocity integrates FIRST, then position uses the new velocity - matching
    // both updateCPU and the compute shader.
    SimParticle p;
    p.life = 1.0f;
    p.startLife = 100.0f;
    p.acceleration = Vec3{0, -10, 0};
    CHECK(simulateParticleStep(p, 0.1f));
    CHECK(approx(p.velocity.y, -1.0f));   // 0 + (-10)(0.1)
    CHECK(approx(p.position.y, -0.1f));   // 0 + (-1)(0.1), i.e. new velocity
}

static void testRotation() {
    SimParticle p;
    p.life = 1.0f;
    p.startLife = 10.0f;
    p.rotationAngle = 1.0f;
    p.rotationSpeed = 2.0f;
    CHECK(simulateParticleStep(p, 0.25f));
    CHECK(approx(p.rotationAngle, 1.5f));
}

static void testBatchCountsAlive() {
    std::vector<SimParticle> ps(5);
    for (int i = 0; i < 5; ++i) {
        ps[static_cast<std::size_t>(i)].life = (i % 2 == 0) ? 1.0f : 0.0f; // 3 alive
        ps[static_cast<std::size_t>(i)].startLife = 10.0f;
    }
    CHECK(simulateParticles(ps.data(), 5, 0.016f) == 3);
}

static void testMultiStepAgainstReference() {
    // 100 steps of the shared step vs an independent transliteration of the CPU
    // loop's formulas: they must agree exactly.
    SimParticle p;
    p.life = 1.0f;
    p.startLife = 3.0f;
    p.position = Vec3{1, 0, -2};
    p.velocity = Vec3{0.5f, 2.0f, 0.0f};
    p.acceleration = Vec3{0.0f, -9.8f, 0.3f};
    p.rotationSpeed = 0.7f;

    float rLife = p.life;
    Vec3 rPos = p.position, rVel = p.velocity;
    float rAngle = p.rotationAngle;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 200; ++i) {
        const bool alive = simulateParticleStep(p, dt);

        // Reference (mirrors ParticleSystem::updateCPU / particle_update.compute).
        bool refAlive = false;
        if (rLife > 0.0f) {
            rLife -= dt / 3.0f;
            if (rLife <= 0.0f) {
                rLife = 0.0f;
            } else {
                rVel += Vec3{0.0f, -9.8f, 0.3f} * dt;
                rPos += rVel * dt;
                rAngle += 0.7f * dt;
                refAlive = true;
            }
        }
        CHECK(alive == refAlive);
        CHECK(approx(p.life, rLife, 1e-6f));
        CHECK(approx(p.position.x, rPos.x, 1e-4f));
        CHECK(approx(p.position.y, rPos.y, 1e-4f));
        CHECK(approx(p.position.z, rPos.z, 1e-4f));
        CHECK(approx(p.rotationAngle, rAngle, 1e-4f));
    }
    CHECK(p.life == 0.0f); // 200 steps at 1/60 s (3.33 s) exceed the 3 s lifetime
}

int main() {
    testDeadParticleUntouched();
    testLifeDecayAndClamp();
    testSemiImplicitEulerOrder();
    testRotation();
    testBatchCountsAlive();
    testMultiStepAgainstReference();

    if (g_failures == 0) {
        std::printf("All particle sim tests passed.\n");
        return 0;
    }
    std::printf("%d particle sim test(s) failed.\n", g_failures);
    return 1;
}
