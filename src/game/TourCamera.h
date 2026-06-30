#pragma once

#include "game/DungeonGame.h"

#include <cmath>

/**
 * @file TourCamera.h
 * @brief First-person "tour" camera to walk inside a generated level (issue #165).
 *
 * Milestone 15. Adds the share-clip "wow" view: the player can switch from the
 * top-down game view to a first-person walkthrough of their map. This is the
 * headless, renderer-agnostic core - eye position, yaw/pitch look, and collision
 * walking that reuses the dungeon's wall collision (#164) - that the engine's
 * OpenGL camera consumes to render the actual view.
 *
 * Header-only and dependency-free (std + the header-only game/world types).
 */
namespace IKore {
namespace game {

enum class CameraMode { TopDown, FirstPerson };

/// A first-person camera: an eye position with yaw/pitch look angles.
struct FirstPersonCamera {
    ecs::Vec3 position{};      ///< eye position (y is eye height).
    float yaw{0.0f};          ///< radians about Y; 0 looks toward +X.
    float pitch{0.0f};        ///< radians up/down, clamped by pitchLimit.
    float eyeHeight{1.6f};
    float lookSensitivity{0.0025f};
    float pitchLimit{1.5f};   ///< ~85 degrees, to avoid gimbal flip.

    /// Full 3D look direction (includes pitch).
    ecs::Vec3 forward() const {
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        return ecs::Vec3{cp * std::cos(yaw), sp, cp * std::sin(yaw)};
    }
    /// Horizontal forward (movement direction on the ground plane).
    ecs::Vec3 forwardHoriz() const { return ecs::Vec3{std::cos(yaw), 0.0f, std::sin(yaw)}; }
    /// Horizontal right (strafe direction).
    ecs::Vec3 rightHoriz() const { return ecs::Vec3{std::sin(yaw), 0.0f, -std::cos(yaw)}; }

    /// Apply a mouse delta to the look angles, clamping pitch.
    void look(float dx, float dy) {
        yaw += dx * lookSensitivity;
        pitch += dy * lookSensitivity;
        if (pitch > pitchLimit) pitch = pitchLimit;
        if (pitch < -pitchLimit) pitch = -pitchLimit;
    }
};

/// Owns the active camera mode and the first-person walkthrough behaviour.
struct TourController {
    CameraMode mode{CameraMode::TopDown};
    FirstPersonCamera camera;
    float moveSpeed{4.0f};
    float radius{0.3f}; ///< collision radius for the walking camera.

    bool isFirstPerson() const { return mode == CameraMode::FirstPerson; }

    /// Enter the first-person walkthrough, placing the eye at the player.
    void enterFirstPerson(const DungeonGame& game) {
        mode = CameraMode::FirstPerson;
        camera.position = ecs::Vec3{game.playerPosition.x, camera.eyeHeight, game.playerPosition.z};
    }
    void exitToTopDown() { mode = CameraMode::TopDown; }

    /// Flip between the two modes (placing the eye at the player when entering FP).
    void toggle(const DungeonGame& game) {
        if (mode == CameraMode::TopDown) {
            enterFirstPerson(game);
        } else {
            exitToTopDown();
        }
    }

    /**
     * @brief Walk the first-person camera relative to its look direction.
     *
     * @p forwardInput / @p strafeInput are in [-1, 1]. Movement is horizontal
     * (the eye stays at eye height) and slides along walls using the dungeon's
     * collision, so the walkthrough stays inside the level.
     */
    void walk(const DungeonGame& game, float forwardInput, float strafeInput, float dt) {
        if (mode != CameraMode::FirstPerson) return;
        const ecs::Vec3 fh = camera.forwardHoriz();
        const ecs::Vec3 rh = camera.rightHoriz();
        const float step = moveSpeed * dt;
        const float dx = (fh.x * forwardInput + rh.x * strafeInput) * step;
        const float dz = (fh.z * forwardInput + rh.z * strafeInput) * step;

        const ecs::Vec3 from = camera.position;
        ecs::Vec3 next = from;
        const ecs::Vec3 full{from.x + dx, camera.eyeHeight, from.z + dz};
        if (!game.hitsWall(full, radius)) {
            next = full;
        } else {
            const ecs::Vec3 xOnly{from.x + dx, camera.eyeHeight, from.z};
            const ecs::Vec3 zOnly{from.x, camera.eyeHeight, from.z + dz};
            if (!game.hitsWall(xOnly, radius)) {
                next = xOnly;
            } else if (!game.hitsWall(zOnly, radius)) {
                next = zOnly;
            }
        }
        next.y = camera.eyeHeight;
        camera.position = next;
    }
};

} // namespace game
} // namespace IKore
