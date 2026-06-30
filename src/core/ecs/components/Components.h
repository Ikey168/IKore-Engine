#pragma once

#include <cmath>
#include <memory>

/**
 * @file Components.h
 * @brief Hot-path components expressed as value types for archetype storage.
 *
 * Milestone 8 (issue #142). These are the frequently-updated components the
 * engine touches every frame - transform, velocity, physics, AI - re-expressed
 * as plain value types so they live in the contiguous, cache-friendly SoA
 * columns provided by the archetype ECS (issues #140/#141) and are driven by the
 * systems in systems/Systems.h.
 *
 * They are intentionally dependency-free (a small built-in Vec3 instead of glm)
 * so the data layer and its systems are unit-testable in isolation. The
 * class-based components under src/core/components/ remain for the parts of the
 * engine not yet migrated; the @ref Legacy bridge below lets an archetype entity
 * carry a link to its legacy counterpart during the transition.
 */
namespace IKore {
namespace ecs {

/// Minimal 3-component vector (kept local to avoid a glm dependency here).
struct Vec3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) {
        x += o.x; y += o.y; z += o.z;
        return *this;
    }
    Vec3& operator*=(float s) {
        x *= s; y *= s; z *= s;
        return *this;
    }

    float lengthSquared() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSquared()); }
    Vec3 normalized() const {
        const float len = length();
        return len > 0.0f ? Vec3{x / len, y / len, z / len} : Vec3{};
    }
};

/// Spatial transform (position, Euler rotation in radians, scale).
struct Transform {
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

/// Linear + angular velocity, integrated into Transform by the movement system.
struct Velocity {
    Vec3 linear{};
    Vec3 angular{};
};

/// Hot physics data integrated into Velocity by the physics system.
struct RigidBody {
    Vec3 acceleration{};   ///< Constant acceleration (e.g. gravity) this step.
    float mass{1.0f};
    float damping{0.0f};   ///< Per-second velocity damping factor in [0, 1].
    bool kinematic{false}; ///< Kinematic bodies are not integrated by physics.
};

/// Hot AI steering data: the agent moves toward @c target at @c speed.
struct AIAgent {
    Vec3 target{};
    float speed{1.0f};
    float arriveRadius{0.05f}; ///< Stop when within this distance of target.
    bool active{true};
};

/**
 * @brief Compatibility bridge to a legacy (class-based) component or entity.
 *
 * During the migration, less-frequently-updated components stay in the existing
 * src/core/components/ classes. Attaching a Legacy to an archetype entity lets
 * it carry a type-erased owning handle to that legacy object (e.g. a
 * std::shared_ptr<Entity> or a specific legacy component), so systems can bridge
 * between the two worlds without the ECS data layer depending on engine types.
 * Ownership rides correctly through archetype moves and is released on destroy.
 */
struct Legacy {
    std::shared_ptr<void> handle;
};

} // namespace ecs
} // namespace IKore
