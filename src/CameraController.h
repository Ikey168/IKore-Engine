#pragma once

#include "Camera.h"
#include <string>

namespace IKore {

    class CameraController {
    public:
        explicit CameraController(Camera& camera);
        
        // Process input for camera movement and controls
        void processInput(GLFWwindow* window, float deltaTime);
        
        // Get current camera status as string (for UI display)
        std::string getCameraStatus() const;
        
        // Enable/disable various camera features
        void setMouseLookEnabled(bool enabled);
        void setMovementEnabled(bool enabled);
        void setFOVControlEnabled(bool enabled);
        
        // Get camera control state
        bool isMouseLookEnabled() const { return mouseLookEnabled; }
        bool isMovementEnabled() const { return movementEnabled; }
        bool isFOVControlEnabled() const { return fovControlEnabled; }
        
        // Set movement speed multiplier
        void setSpeedMultiplier(float multiplier) { speedMultiplier = multiplier; }
        float getSpeedMultiplier() const { return speedMultiplier; }
        
    private:
        Camera& camera;
        bool mouseLookEnabled;
        bool movementEnabled;
        bool fovControlEnabled;
        float speedMultiplier;
        
        // Track key states for smooth FOV adjustment
        bool qKeyPressed;
        bool eKeyPressed;
    };

} // namespace IKore
