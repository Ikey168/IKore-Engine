#pragma once

#include "core/ecs/components/Components.h" // ecs::Vec3

/**
 * @file ParticleSim.h
 * @brief The per-particle update step shared by the CPU and GPU particle paths
 *        (issue #259, renderer P-completions).
 *
 * ParticleSystem::updateCPU and src/shaders/particle_update.compute must advance a
 * particle identically or toggling the GPU path would change the look. This header
 * is that step as renderer-agnostic, unit-testable math - the single reference both
 * mirror:
 *
 *   life -= dt / startLife           (die at life <= 0)
 *   velocity += acceleration * dt    (semi-implicit Euler)
 *   position += velocity * dt
 *   rotation.angle += rotation.speed * dt
 *
 * Header-only, std + the engine's glm-free value types.
 */
namespace IKore {
namespace render {

/// A particle's simulated state (the fields the update step touches).
struct SimParticle {
    ecs::Vec3 position{};
    ecs::Vec3 velocity{};
    ecs::Vec3 acceleration{};
    float life{0.0f};      ///< normalized [0, 1]; dead at <= 0.
    float startLife{1.0f}; ///< seconds of total life (scales the decay rate).
    float rotationAngle{0.0f};
    float rotationSpeed{0.0f};
};

/**
 * @brief Advance one particle by @p dt seconds. Returns true while it stays alive.
 *
 * Dead particles (life <= 0 on entry) are left untouched. A particle whose life
 * expires this step has its life clamped to exactly 0 and its motion is not
 * advanced further, matching both the CPU loop and the compute shader.
 */
inline bool simulateParticleStep(SimParticle& p, float dt) {
    if (p.life <= 0.0f) return false;

    p.life -= p.startLife > 0.0f ? dt / p.startLife : 1.0f;
    if (p.life <= 0.0f) {
        p.life = 0.0f;
        return false;
    }

    p.velocity += p.acceleration * dt;
    p.position += p.velocity * dt;
    p.rotationAngle += p.rotationSpeed * dt;
    return true;
}

/// Advance an array of particles; returns how many are still alive (the same
/// count ParticleSystem tracks as its active-particle total).
inline int simulateParticles(SimParticle* particles, int count, float dt) {
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (simulateParticleStep(particles[i], dt)) ++alive;
    }
    return alive;
}

} // namespace render
} // namespace IKore
