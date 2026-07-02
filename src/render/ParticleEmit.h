#pragma once

/**
 * @file ParticleEmit.h
 * @brief Emitter cadence + live-count estimate shared by the CPU and GPU particle
 *        paths (issue #267).
 *
 * Moving emission onto the GPU must not change emitter behavior (rate, bursts,
 * looping) from the user's perspective. The decisions that must stay identical across
 * the two paths are pure scalar bookkeeping, so they live here as unit-testable math
 * that both ParticleSystem::emitParticles (CPU) and the GPU dispatch call:
 *
 *   - particlesToEmit(): how many whole particles to spawn this step, carrying the
 *     sub-particle fraction in a caller-owned accumulator so a fractional rate still
 *     emits at the right average cadence.
 *   - emissionFinished(): whether a non-looping emitter has stopped emitting.
 *   - estimateActiveParticles(): a readback-free running estimate of the live count.
 *     Rendering now reads the particle buffer directly (no glGetBufferSubData), so the
 *     exact count is no longer pulled back from the GPU; this tracks it on the CPU via
 *     the same birth/death rates, converging to emissionRate * startLife at steady
 *     state (emissionRate*dt in == active*dt/startLife out).
 *
 * Header-only, std only.
 */
namespace IKore {
namespace render {

/**
 * @brief Whole particles to emit this step for a (possibly fractional) rate.
 * @param emissionRate Particles per second (negative treated as 0).
 * @param dt           Seconds elapsed this step.
 * @param[in,out] remainder Sub-particle carry; updated with the leftover fraction.
 * @return Count in [0, ...] to emit now.
 *
 * Accumulates rate*dt plus the prior remainder, emits the whole part, and carries the
 * fraction forward so, e.g., 0.4 particles/step emits every ~2-3 steps on average.
 */
inline int particlesToEmit(float emissionRate, float dt, float& remainder) {
    float toEmit = emissionRate * dt + remainder;
    if (toEmit < 0.0f) toEmit = 0.0f;
    int whole = static_cast<int>(toEmit);
    remainder = toEmit - static_cast<float>(whole);
    return whole;
}

/// Whether a non-looping emitter has finished emitting (its run exceeded startLife).
/// A looping emitter never finishes. Mirrors the guard in ParticleSystem::emitParticles.
inline bool emissionFinished(bool looping, float emissionTimer, float startLife) {
    return !looping && emissionTimer > startLife;
}

/**
 * @brief Advance a readback-free estimate of the live particle count.
 * @param currentActive Previous estimate (carried as a float to avoid per-step
 *                      rounding drift; round only when reporting the count).
 * @param emitted       Particles emitted this step.
 * @param dt            Seconds elapsed this step.
 * @param startLife     Mean particle lifetime in seconds (<= 0 applies no decay).
 * @param maxParticles  Capacity clamp.
 * @return New estimate in [0, maxParticles].
 *
 * Births add @p emitted; deaths remove the fraction of the pool whose life ran out
 * this step (active * dt / startLife), the same decay the update step applies per
 * particle. At steady state this settles at emissionRate * startLife.
 */
inline float estimateActiveParticles(float currentActive, int emitted, float dt,
                                     float startLife, int maxParticles) {
    float active = currentActive + static_cast<float>(emitted);
    if (startLife > 0.0f) {
        active -= currentActive * (dt / startLife);
    }
    if (active < 0.0f) active = 0.0f;
    const float cap = static_cast<float>(maxParticles);
    if (active > cap) active = cap;
    return active;
}

} // namespace render
} // namespace IKore
