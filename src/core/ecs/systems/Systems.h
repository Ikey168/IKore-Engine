#pragma once

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"
#include "core/ecs/components/Components.h"

/**
 * @file Systems.h
 * @brief Hot-path systems that run over archetype storage via the query API.
 *
 * Milestone 8 (issue #142). These are the per-frame updates for the migrated
 * components. Each is a plain free function that iterates the relevant
 * components with Registry::view<...>().each(...), so it walks the contiguous
 * SoA columns directly - the cache-friendly hot path.
 *
 * Pipeline order (see @ref stepSimulation): AI steering sets desired velocities,
 * physics integrates accelerations into velocities, then movement integrates
 * velocities into transforms. Entities typically carry either AIAgent or
 * RigidBody, so the two velocity writers rarely overlap; the order is fixed for
 * determinism regardless.
 */
namespace IKore {
namespace ecs {

/// Integrate RigidBody acceleration (and damping) into Velocity.
inline void physicsSystem(Registry& registry, float dt) {
    registry.view<Velocity, RigidBody>().each([dt](Entity, Velocity& v, RigidBody& rb) {
        if (rb.kinematic) return;
        v.linear += rb.acceleration * dt;
        if (rb.damping > 0.0f) {
            float factor = 1.0f - rb.damping * dt;
            if (factor < 0.0f) factor = 0.0f;
            v.linear *= factor;
            v.angular *= factor;
        }
    });
}

/// Steer each AIAgent toward its target by writing its Velocity.
inline void aiSteeringSystem(Registry& registry) {
    registry.view<Transform, Velocity, AIAgent>().each(
        [](Entity, Transform& t, Velocity& v, AIAgent& a) {
            if (!a.active) {
                v.linear = Vec3{};
                return;
            }
            const Vec3 toTarget = a.target - t.position;
            if (toTarget.length() > a.arriveRadius) {
                v.linear = toTarget.normalized() * a.speed;
            } else {
                v.linear = Vec3{}; // arrived: hold position
            }
        });
}

/// Integrate Velocity into Transform (position and Euler rotation).
inline void movementSystem(Registry& registry, float dt) {
    registry.view<Transform, Velocity>().each([dt](Entity, Transform& t, Velocity& v) {
        t.position += v.linear * dt;
        t.rotation += v.angular * dt;
    });
}

/// Run the full hot-path pipeline for one fixed step of @p dt seconds.
inline void stepSimulation(Registry& registry, float dt) {
    physicsSystem(registry, dt);
    aiSteeringSystem(registry);
    movementSystem(registry, dt);
}

} // namespace ecs
} // namespace IKore
