#include "CameraController.h"
#include "core/Logger.h"
#include <sstream>
#include <iomanip>

namespace IKore {

    CameraController::CameraController(Camera& cam)
        : camera(cam), mouseLookEnabled(true), movementEnabled(true), 
          fovControlEnabled(true), speedMultiplier(1.0f), 
          qKeyPressed(false), eKeyPressed(false) {
        LOG_INFO("CameraController initialized");
    }

    void CameraController::processInput(GLFWwindow* window, float deltaTime) {
        // Exit control
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
            return;
        }

        // Toggle mouse look with Tab key
        static bool tabKeyWasPressed = false;
        bool tabKeyIsPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabKeyIsPressed && !tabKeyWasPressed) {
            mouseLookEnabled = !mouseLookEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, 
                           mouseLookEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            LOG_INFO("Mouse look " + std::string(mouseLookEnabled ? "enabled" : "disabled"));
        }
        tabKeyWasPressed = tabKeyIsPressed;

        if (!movementEnabled) return;

        // Movement controls with speed multiplier
        float adjustedSpeed = speedMultiplier;
        
        // Sprint with Left Ctrl
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            adjustedSpeed *= 3.0f;
        }
        
        // Slow movement with Alt
        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
            adjustedSpeed *= 0.3f;
        }

        // Save original speed, apply multiplier, restore after movement
        float originalSpeed = camera.movementSpeed;
        camera.movementSpeed *= adjustedSpeed;

        // Standard movement
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::UP, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera.processKeyboard(CameraMovement::DOWN, deltaTime);

        // Restore original speed
        camera.movementSpeed = originalSpeed;

        if (!fovControlEnabled) return;

        // FOV adjustment with Q/E keys
        bool qKeyIsPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
        bool eKeyIsPressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

        if (qKeyIsPressed && !qKeyPressed) {
            float currentFOV = camera.getFOV();
            camera.setFOV(currentFOV - 5.0f); // Decrease FOV by 5 degrees
        }
        if (eKeyIsPressed && !eKeyPressed) {
            float currentFOV = camera.getFOV();
            camera.setFOV(currentFOV + 5.0f); // Increase FOV by 5 degrees
        }

        qKeyPressed = qKeyIsPressed;
        eKeyPressed = eKeyIsPressed;

        // Reset FOV with R key
        static bool rKeyWasPressed = false;
        bool rKeyIsPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (rKeyIsPressed && !rKeyWasPressed) {
            camera.setFOV(45.0f); // Reset to default FOV
            LOG_INFO("FOV reset to default (45 degrees)");
        }
        rKeyWasPressed = rKeyIsPressed;
    }

    std::string CameraController::getCameraStatus() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        glm::vec3 pos = camera.getPosition();
        oss << "Camera Status:\n";
        oss << "Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
        oss << "FOV: " << camera.getFOV() << "°\n";
        oss << "Speed: " << camera.movementSpeed << " (x" << speedMultiplier << ")\n";
        oss << "Mouse Look: " << (mouseLookEnabled ? "ON" : "OFF") << "\n";
        oss << "Movement: " << (movementEnabled ? "ON" : "OFF") << "\n";
        oss << "FOV Control: " << (fovControlEnabled ? "ON" : "OFF");
        
        return oss.str();
    }

    void CameraController::setMouseLookEnabled(bool enabled) {
        mouseLookEnabled = enabled;
        LOG_INFO("Mouse look " + std::string(enabled ? "enabled" : "disabled"));
    }

    void CameraController::setMovementEnabled(bool enabled) {
        movementEnabled = enabled;
        LOG_INFO("Camera movement " + std::string(enabled ? "enabled" : "disabled"));
    }

    void CameraController::setFOVControlEnabled(bool enabled) {
        fovControlEnabled = enabled;
        LOG_INFO("FOV control " + std::string(enabled ? "enabled" : "disabled"));
    }

} // namespace IKore
