#include "Camera.h"
#include "Logger.h"
#include <algorithm>

namespace IKore {

    Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
        : position(position), worldUp(up), yaw(yaw), pitch(pitch),
          movementSpeed(SPEED), mouseSensitivity(SENSITIVITY), fov(FOV),
          nearPlane(NEAR_PLANE), farPlane(FAR_PLANE), aspectRatio(16.0f / 9.0f) {
        updateCameraVectors();
        LOG_INFO("Camera initialized at position: (" + 
                std::to_string(position.x) + ", " + 
                std::to_string(position.y) + ", " + 
                std::to_string(position.z) + ")");
    }

    Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
        : position(glm::vec3(posX, posY, posZ)), worldUp(glm::vec3(upX, upY, upZ)),
          yaw(yaw), pitch(pitch), movementSpeed(SPEED), mouseSensitivity(SENSITIVITY),
          fov(FOV), nearPlane(NEAR_PLANE), farPlane(FAR_PLANE), aspectRatio(16.0f / 9.0f) {
        updateCameraVectors();
        LOG_INFO("Camera initialized at position: (" + 
                std::to_string(posX) + ", " + 
                std::to_string(posY) + ", " + 
                std::to_string(posZ) + ")");
    }

    glm::mat4 Camera::getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 Camera::getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    void Camera::processKeyboard(CameraMovement direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        
        switch (direction) {
            case CameraMovement::FORWARD:
                position += front * velocity;
                break;
            case CameraMovement::BACKWARD:
                position -= front * velocity;
                break;
            case CameraMovement::LEFT:
                position -= right * velocity;
                break;
            case CameraMovement::RIGHT:
                position += right * velocity;
                break;
            case CameraMovement::UP:
                position += up * velocity;
                break;
            case CameraMovement::DOWN:
                position -= up * velocity;
                break;
        }
    }

    void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        yaw += xOffset;
        pitch += yOffset;

        // Make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch) {
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }

        // Update front, right and up vectors using the updated Euler angles
        updateCameraVectors();
    }

    void Camera::processMouseScroll(float yOffset) {
        fov -= yOffset;
        if (fov < 1.0f)
            fov = 1.0f;
        if (fov > 120.0f)
            fov = 120.0f;
        
        LOG_INFO("FOV adjusted to: " + std::to_string(fov));
    }

    void Camera::setAspectRatio(float aspect) {
        aspectRatio = aspect;
        LOG_INFO("Camera aspect ratio set to: " + std::to_string(aspect));
    }

    void Camera::setFOV(float newFov) {
        fov = std::clamp(newFov, 1.0f, 120.0f);
        LOG_INFO("Camera FOV set to: " + std::to_string(fov));
    }

    void Camera::setPlanes(float near, float far) {
        nearPlane = near;
        farPlane = far;
        LOG_INFO("Camera planes set - Near: " + std::to_string(near) + ", Far: " + std::to_string(far));
    }

    void Camera::lookAt(const glm::vec3& target) {
        glm::vec3 direction = glm::normalize(target - position);
        
        // Calculate yaw and pitch from direction vector
        yaw = glm::degrees(atan2(direction.z, direction.x));
        pitch = glm::degrees(asin(direction.y));
        
        updateCameraVectors();
        LOG_INFO("Camera looking at target: (" + 
                std::to_string(target.x) + ", " + 
                std::to_string(target.y) + ", " + 
                std::to_string(target.z) + ")");
    }

    void Camera::setPosition(const glm::vec3& pos) {
        position = pos;
        LOG_INFO("Camera position set to: (" + 
                std::to_string(pos.x) + ", " + 
                std::to_string(pos.y) + ", " + 
                std::to_string(pos.z) + ")");
    }

    void Camera::updateCameraVectors() {
        // Calculate the new front vector
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
        
        // Also re-calculate the right and up vector
        // Normalize the vectors, because their length gets closer to 0 the more you look up or down
        // which results in slower movement.
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }

} // namespace IKore
