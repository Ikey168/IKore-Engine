#include "ControllableEntities.h"
#include "Logger.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace IKore {

    // =============================================================================
    // ControllableGameObject Implementation
    // =============================================================================

    ControllableGameObject::ControllableGameObject(const std::string& name, 
                                                   const glm::vec3& position,
                                                   MovementMode movementMode)
        : TransformableGameObject(name, position)
        , m_inputComponent("ControllableGameObject_" + name)
        , m_movementMode(movementMode)
        , m_movementSpeed(5.0f)
        , m_rotationSpeed(0.1f)
        , m_mouseCaptured(false)
        , m_movementDirection(0.0f)
        , m_rotation(0.0f)
        , m_movingForward(false)
        , m_movingBackward(false)
        , m_movingLeft(false)
        , m_movingRight(false)
        , m_movingUp(false)
        , m_movingDown(false)
    {
        LOG_INFO("Created ControllableGameObject: " + name);
    }

    void ControllableGameObject::initialize() {
        TransformableGameObject::initialize();
        
        // Initialize input component
        m_inputComponent.initialize();
        
        // Setup input bindings
        setupInputBindings();
        
        LOG_INFO("Initialized ControllableGameObject: " + getName());
    }

    void ControllableGameObject::update(float deltaTime) {
        TransformableGameObject::update(deltaTime);
        
        // Update input component
        m_inputComponent.update(deltaTime);
        
        // Calculate movement vector based on current input state
        glm::vec3 movement(0.0f);
        
        if (m_movingForward) movement.z -= 1.0f;
        if (m_movingBackward) movement.z += 1.0f;
        if (m_movingLeft) movement.x -= 1.0f;
        if (m_movingRight) movement.x += 1.0f;
        
        // Handle vertical movement for flying modes
        if (m_movementMode == MovementMode::FLYING || m_movementMode == MovementMode::CAMERA_STYLE) {
            if (m_movingUp) movement.y += 1.0f;
            if (m_movingDown) movement.y -= 1.0f;
        }
        
        // Normalize movement vector if needed
        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement);
            
            // For camera-style movement, apply rotation to movement vector
            if (m_movementMode == MovementMode::CAMERA_STYLE) {
                // Create rotation matrix from current rotation
                glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                
                // Transform movement vector by rotation
                glm::vec4 rotatedMovement = rotationMatrix * glm::vec4(movement, 0.0f);
                movement = glm::vec3(rotatedMovement);
            }
            
            // Apply movement to transform
            glm::vec3 currentPosition = getTransform().getPosition();
            glm::vec3 newPosition = currentPosition + movement * m_movementSpeed * deltaTime;
            
            // For walking mode, constrain Y movement
            if (m_movementMode == MovementMode::WALKING) {
                newPosition.y = currentPosition.y;
            }
            
            getTransform().setPosition(newPosition);
        }
        
        // Apply rotation for camera-style movement
        if (m_movementMode == MovementMode::CAMERA_STYLE) {
            glm::vec3 euler(m_rotation.x, m_rotation.y, 0.0f);
            getTransform().setRotation(euler);
        }
    }

    void ControllableGameObject::cleanup() {
        m_inputComponent.cleanup();
        TransformableGameObject::cleanup();
        
        LOG_INFO("Cleaned up ControllableGameObject: " + getName());
    }

    void ControllableGameObject::setMovementMode(MovementMode mode) {
        if (m_movementMode != mode) {
            m_movementMode = mode;
            
            // Reset movement state
            m_movingForward = m_movingBackward = m_movingLeft = m_movingRight = false;
            m_movingUp = m_movingDown = false;
            m_rotation = glm::vec2(0.0f);
            
            // Re-setup input bindings for new mode
            setupInputBindings();
            
            LOG_INFO("Changed movement mode for " + getName());
        }
    }

    void ControllableGameObject::setMouseCapture(bool capture) {
        m_mouseCaptured = capture;
        
        // Enable/disable mouse cursor based on capture state
        InputManager& inputManager = InputManager::getInstance();
        if (capture) {
            inputManager.setCursorMode(GLFW_CURSOR_DISABLED);
        } else {
            inputManager.setCursorMode(GLFW_CURSOR_NORMAL);
        }
    }

    void ControllableGameObject::setupInputBindings() {
        // Clear existing bindings
        m_inputComponent.clearBindings();
        
        // Movement bindings (WASD)
        m_inputComponent.bindKeyAction(GLFW_KEY_W, [this](const InputEvent& event) {
            m_movingForward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_S, [this](const InputEvent& event) {
            m_movingBackward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_A, [this](const InputEvent& event) {
            m_movingLeft = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_D, [this](const InputEvent& event) {
            m_movingRight = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        // Vertical movement for flying modes
        if (m_movementMode == MovementMode::FLYING || m_movementMode == MovementMode::CAMERA_STYLE) {
            m_inputComponent.bindKeyAction(GLFW_KEY_SPACE, [this](const InputEvent& event) {
                m_movingUp = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
            });
            
            m_inputComponent.bindKeyAction(GLFW_KEY_LEFT_SHIFT, [this](const InputEvent& event) {
                m_movingDown = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
            });
        }
        
        // Mouse look for camera-style movement
        if (m_movementMode == MovementMode::CAMERA_STYLE) {
            m_inputComponent.bindMouseMoveAction([this](const InputEvent& event) {
                if (m_mouseCaptured) {
                    handleMouseLook(event);
                }
            });
            
            // Toggle mouse capture with ESC key
            m_inputComponent.bindKeyAction(GLFW_KEY_ESCAPE, [this](const InputEvent& event) {
                if (event.action == InputActionType::PRESS) {
                    setMouseCapture(!m_mouseCaptured);
                }
            });
        }
        
        // Action bindings
        m_inputComponent.bindKeyAction(GLFW_KEY_E, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS) {
                handleActionInput(event);
            }
        });
        
        // Mouse scroll for speed adjustment
        m_inputComponent.bindScrollAction([this](const InputEvent& event) {
            handleMouseScroll(event);
        });
        
        LOG_INFO("Setup input bindings for " + getName());
    }

    void ControllableGameObject::handleMovementInput(const InputEvent& event) {
        // Movement is handled in the key action callbacks
        // This method can be overridden for custom movement logic
    }

    void ControllableGameObject::handleMouseLook(const InputEvent& event) {
        if (event.type == InputEventType::MOUSE_MOVE) {
            float xOffset = event.mouseData.deltaX * m_rotationSpeed;
            float yOffset = event.mouseData.deltaY * m_rotationSpeed;
            
            m_rotation.y += xOffset;  // Yaw
            m_rotation.x += yOffset;  // Pitch
            
            // Constrain pitch
            m_rotation.x = std::clamp(m_rotation.x, -89.0f, 89.0f);
        }
    }

    void ControllableGameObject::handleActionInput(const InputEvent& event) {
        LOG_INFO(getName() + " performed action!");
    }

    void ControllableGameObject::handleMouseScroll(const InputEvent& event) {
        if (event.type == InputEventType::MOUSE_SCROLL) {
            // Adjust movement speed with scroll wheel
            float speedDelta = event.scrollData.yOffset * 0.5f;
            m_movementSpeed = std::max(0.1f, m_movementSpeed + speedDelta);
            
            LOG_INFO(getName() + " speed: " + std::to_string(m_movementSpeed));
        }
    }

    // =============================================================================
    // ControllableCamera Implementation
    // =============================================================================

    ControllableCamera::ControllableCamera(const std::string& name,
                                           const glm::vec3& position,
                                           CameraMode mode)
        : TransformableCamera(name, position)
        , m_inputComponent("ControllableCamera_" + name)
        , m_cameraMode(mode)
        , m_movementSpeed(5.0f)
        , m_mouseSensitivity(0.1f)
        , m_target(0.0f)
        , m_orbitDistance(5.0f)
        , m_orbitAngles(0.0f)
        , m_rotation(0.0f)
        , m_movingForward(false)
        , m_movingBackward(false)
        , m_movingLeft(false)
        , m_movingRight(false)
        , m_movingUp(false)
        , m_movingDown(false)
    {
        LOG_INFO("Created ControllableCamera: " + name);
    }

    void ControllableCamera::initialize() {
        TransformableCamera::initialize();
        
        // Initialize input component
        m_inputComponent.initialize();
        
        // Setup input bindings
        setupInputBindings();
        
        LOG_INFO("Initialized ControllableCamera: " + getName());
    }

    void ControllableCamera::update(float deltaTime) {
        TransformableCamera::update(deltaTime);
        
        // Update input component
        m_inputComponent.update(deltaTime);
        
        if (m_cameraMode == CameraMode::THIRD_PERSON) {
            updateThirdPersonPosition();
        } else {
            // Handle first-person and free-fly movement
            glm::vec3 movement(0.0f);
            
            if (m_movingForward) movement.z -= 1.0f;
            if (m_movingBackward) movement.z += 1.0f;
            if (m_movingLeft) movement.x -= 1.0f;
            if (m_movingRight) movement.x += 1.0f;
            if (m_movingUp) movement.y += 1.0f;
            if (m_movingDown) movement.y -= 1.0f;
            
            // Normalize movement vector if needed
            if (glm::length(movement) > 0.0f) {
                movement = glm::normalize(movement);
                
                // Apply rotation to movement vector for first-person mode
                if (m_cameraMode == CameraMode::FIRST_PERSON) {
                    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::vec4 rotatedMovement = rotationMatrix * glm::vec4(movement, 0.0f);
                    movement = glm::vec3(rotatedMovement);
                }
                
                // Apply movement to transform
                glm::vec3 currentPosition = getTransform().getPosition();
                glm::vec3 newPosition = currentPosition + movement * m_movementSpeed * deltaTime;
                getTransform().setPosition(newPosition);
            }
            
            // Apply rotation
            glm::vec3 euler(m_rotation.x, m_rotation.y, 0.0f);
            getTransform().setRotation(euler);
        }
    }

    void ControllableCamera::cleanup() {
        m_inputComponent.cleanup();
        TransformableCamera::cleanup();
        
        LOG_INFO("Cleaned up ControllableCamera: " + getName());
    }

    void ControllableCamera::setCameraMode(CameraMode mode) {
        if (m_cameraMode != mode) {
            m_cameraMode = mode;
            
            // Reset movement state
            m_movingForward = m_movingBackward = m_movingLeft = m_movingRight = false;
            m_movingUp = m_movingDown = false;
            m_rotation = glm::vec2(0.0f);
            m_orbitAngles = glm::vec2(0.0f);
            
            // Re-setup input bindings for new mode
            setupInputBindings();
            
            LOG_INFO("Changed camera mode for " + getName());
        }
    }

    void ControllableCamera::setupInputBindings() {
        // Clear existing bindings
        m_inputComponent.clearBindings();
        
        // Movement bindings (WASD)
        m_inputComponent.bindKeyAction(GLFW_KEY_W, [this](const InputEvent& event) {
            m_movingForward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_S, [this](const InputEvent& event) {
            m_movingBackward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_A, [this](const InputEvent& event) {
            m_movingLeft = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_D, [this](const InputEvent& event) {
            m_movingRight = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        // Vertical movement
        m_inputComponent.bindKeyAction(GLFW_KEY_SPACE, [this](const InputEvent& event) {
            m_movingUp = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_LEFT_SHIFT, [this](const InputEvent& event) {
            m_movingDown = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        // Mouse look
        m_inputComponent.bindMouseMoveAction([this](const InputEvent& event) {
            handleMouseLook(event);
        });
        
        // Mouse scroll for zoom/FOV adjustment
        m_inputComponent.bindScrollAction([this](const InputEvent& event) {
            handleMouseScroll(event);
        });
        
        // Camera mode switching
        m_inputComponent.bindKeyAction(GLFW_KEY_1, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS) {
                setCameraMode(CameraMode::FIRST_PERSON);
            }
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_2, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS) {
                setCameraMode(CameraMode::THIRD_PERSON);
            }
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_3, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS) {
                setCameraMode(CameraMode::FREE_FLY);
            }
        });
        
        LOG_INFO("Setup camera input bindings for " + getName());
    }

    void ControllableCamera::handleMovementInput(const InputEvent& event) {
        // Movement is handled in the key action callbacks
    }

    void ControllableCamera::handleMouseLook(const InputEvent& event) {
        if (event.type == InputEventType::MOUSE_MOVE) {
            float xOffset = event.mouseData.deltaX * m_mouseSensitivity;
            float yOffset = event.mouseData.deltaY * m_mouseSensitivity;
            
            if (m_cameraMode == CameraMode::THIRD_PERSON) {
                m_orbitAngles.y += xOffset;  // Yaw
                m_orbitAngles.x += yOffset;  // Pitch
                
                // Constrain pitch for third-person
                m_orbitAngles.x = std::clamp(m_orbitAngles.x, -89.0f, 89.0f);
            } else {
                m_rotation.y += xOffset;  // Yaw
                m_rotation.x += yOffset;  // Pitch
                
                // Constrain pitch
                m_rotation.x = std::clamp(m_rotation.x, -89.0f, 89.0f);
            }
        }
    }

    void ControllableCamera::handleMouseScroll(const InputEvent& event) {
        if (event.type == InputEventType::MOUSE_SCROLL) {
            if (m_cameraMode == CameraMode::THIRD_PERSON) {
                // Adjust orbit distance
                float distanceDelta = event.scrollData.yOffset * 0.5f;
                m_orbitDistance = std::max(1.0f, m_orbitDistance - distanceDelta);
            } else {
                // Adjust movement speed
                float speedDelta = event.scrollData.yOffset * 0.5f;
                m_movementSpeed = std::max(0.1f, m_movementSpeed + speedDelta);
            }
            
            LOG_INFO(getName() + " scroll adjustment");
        }
    }

    void ControllableCamera::updateThirdPersonPosition() {
        // Calculate position based on orbit angles and distance
        float yaw = glm::radians(m_orbitAngles.y);
        float pitch = glm::radians(m_orbitAngles.x);
        
        glm::vec3 offset;
        offset.x = m_orbitDistance * cos(pitch) * sin(yaw);
        offset.y = m_orbitDistance * sin(pitch);
        offset.z = m_orbitDistance * cos(pitch) * cos(yaw);
        
        glm::vec3 position = m_target + offset;
        getTransform().setPosition(position);
        
        // Look at target
        glm::vec3 direction = glm::normalize(m_target - position);
        float yawAngle = atan2(direction.x, direction.z);
        float pitchAngle = asin(-direction.y);
        
        glm::vec3 rotation(glm::degrees(pitchAngle), glm::degrees(yawAngle), 0.0f);
        getTransform().setRotation(rotation);
    }

    // =============================================================================
    // ControllableLight Implementation
    // =============================================================================

    ControllableLight::ControllableLight(const std::string& name,
                                         LightType type,
                                         const glm::vec3& position,
                                         const glm::vec3& color,
                                         float intensity)
        : TransformableLight(name, type, position, color, intensity)
        , m_inputComponent("ControllableLight_" + name)
        , m_movementSpeed(2.0f)
        , m_movingForward(false)
        , m_movingBackward(false)
        , m_movingLeft(false)
        , m_movingRight(false)
        , m_movingUp(false)
        , m_movingDown(false)
    {
        LOG_INFO("Created ControllableLight: " + name);
    }

    void ControllableLight::initialize() {
        TransformableLight::initialize();
        
        // Initialize input component
        m_inputComponent.initialize();
        
        // Setup input bindings
        setupInputBindings();
        
        LOG_INFO("Initialized ControllableLight: " + getName());
    }

    void ControllableLight::update(float deltaTime) {
        TransformableLight::update(deltaTime);
        
        // Update input component
        m_inputComponent.update(deltaTime);
        
        // Calculate movement vector based on current input state
        glm::vec3 movement(0.0f);
        
        if (m_movingForward) movement.z -= 1.0f;
        if (m_movingBackward) movement.z += 1.0f;
        if (m_movingLeft) movement.x -= 1.0f;
        if (m_movingRight) movement.x += 1.0f;
        if (m_movingUp) movement.y += 1.0f;
        if (m_movingDown) movement.y -= 1.0f;
        
        // Apply movement if any
        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement);
            glm::vec3 currentPosition = getTransform().getPosition();
            glm::vec3 newPosition = currentPosition + movement * m_movementSpeed * deltaTime;
            getTransform().setPosition(newPosition);
        }
    }

    void ControllableLight::cleanup() {
        m_inputComponent.cleanup();
        TransformableLight::cleanup();
        
        LOG_INFO("Cleaned up ControllableLight: " + getName());
    }

    void ControllableLight::setupInputBindings() {
        // Clear existing bindings
        m_inputComponent.clearBindings();
        
        // Movement bindings (Arrow keys for lights to avoid conflicts)
        m_inputComponent.bindKeyAction(GLFW_KEY_UP, [this](const InputEvent& event) {
            m_movingForward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_DOWN, [this](const InputEvent& event) {
            m_movingBackward = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_LEFT, [this](const InputEvent& event) {
            m_movingLeft = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_RIGHT, [this](const InputEvent& event) {
            m_movingRight = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_PAGE_UP, [this](const InputEvent& event) {
            m_movingUp = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_PAGE_DOWN, [this](const InputEvent& event) {
            m_movingDown = (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT);
        });
        
        // Light property adjustment bindings
        m_inputComponent.bindKeyAction(GLFW_KEY_EQUAL, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT) {
                // Increase intensity
                float currentIntensity = getIntensity();
                setIntensity(std::min(10.0f, currentIntensity + 0.1f));
                LOG_INFO(getName() + " intensity: " + std::to_string(getIntensity()));
            }
        });
        
        m_inputComponent.bindKeyAction(GLFW_KEY_MINUS, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS || event.action == InputActionType::REPEAT) {
                // Decrease intensity
                float currentIntensity = getIntensity();
                setIntensity(std::max(0.0f, currentIntensity - 0.1f));
                LOG_INFO(getName() + " intensity: " + std::to_string(getIntensity()));
            }
        });
        
        // Color cycling
        m_inputComponent.bindKeyAction(GLFW_KEY_C, [this](const InputEvent& event) {
            if (event.action == InputActionType::PRESS) {
                handleLightAdjustment(event);
            }
        });
        
        LOG_INFO("Setup light input bindings for " + getName());
    }

    void ControllableLight::handleMovementInput(const InputEvent& event) {
        // Movement is handled in the key action callbacks
    }

    void ControllableLight::handleLightAdjustment(const InputEvent& event) {
        // Cycle through different light colors
        static int colorIndex = 0;
        static const glm::vec3 colors[] = {
            glm::vec3(1.0f, 1.0f, 1.0f),  // White
            glm::vec3(1.0f, 0.0f, 0.0f),  // Red
            glm::vec3(0.0f, 1.0f, 0.0f),  // Green
            glm::vec3(0.0f, 0.0f, 1.0f),  // Blue
            glm::vec3(1.0f, 1.0f, 0.0f),  // Yellow
            glm::vec3(1.0f, 0.0f, 1.0f),  // Magenta
            glm::vec3(0.0f, 1.0f, 1.0f)   // Cyan
        };
        static const int numColors = sizeof(colors) / sizeof(colors[0]);
        
        colorIndex = (colorIndex + 1) % numColors;
        setColor(colors[colorIndex]);
        
        LOG_INFO(getName() + " changed color");
    }

} // namespace IKore
