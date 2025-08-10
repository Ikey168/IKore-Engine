#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace IKore {

    // Camera movement directions
    enum class CameraMovement {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    // Default camera values
    const float YAW = -90.0f;
    const float PITCH = 0.0f;
    const float SPEED = 2.5f;
    const float SENSITIVITY = 0.1f;
    const float FOV = 45.0f;
    const float NEAR_PLANE = 0.1f;
    const float FAR_PLANE = 100.0f;

    class Camera {
    public:
        // Camera attributes
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        glm::vec3 right;
        glm::vec3 worldUp;

        // Euler angles
        float yaw;
        float pitch;

        // Camera options
        float movementSpeed;
        float mouseSensitivity;
        float fov;
        float nearPlane;
        float farPlane;
        float aspectRatio;

        // Constructor with vectors
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
               glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
               float yaw = YAW,
               float pitch = PITCH);

        // Constructor with scalar values
        Camera(float posX, float posY, float posZ,
               float upX, float upY, float upZ,
               float yaw, float pitch);

        // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
        glm::mat4 getViewMatrix() const;

        // Returns the perspective projection matrix
        glm::mat4 getProjectionMatrix() const;

        // Processes input received from any keyboard-like input system
        void processKeyboard(CameraMovement direction, float deltaTime);

        // Processes input received from a mouse input system
        void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);

        // Processes input received from a mouse scroll-wheel event
        void processMouseScroll(float yOffset);

        // Set aspect ratio (typically width/height)
        void setAspectRatio(float aspect);

        // Set field of view
        void setFOV(float fov);

        // Get current FOV
        float getFOV() const { return fov; }

        // Set near and far planes
        void setPlanes(float near, float far);

        // Update camera position to look at a specific target
        void lookAt(const glm::vec3& target);

        // Set camera position
        void setPosition(const glm::vec3& pos);

        // Get camera position
        const glm::vec3& getPosition() const { return position; }

        // Get camera front vector
        const glm::vec3& getFront() const { return front; }

        // Get camera up vector
        const glm::vec3& getUp() const { return up; }

        // Get camera right vector
        const glm::vec3& getRight() const { return right; }

    private:
        // Calculates the front vector from the Camera's (updated) Euler Angles
        void updateCameraVectors();
    };

} // namespace IKore
