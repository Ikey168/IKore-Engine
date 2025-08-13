#include "InputComponent.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

namespace IKore {

    // ============================================================================
    // InputManager Implementation
    // ============================================================================

    InputManager& InputManager::getInstance() {
        static InputManager instance;
        return instance;
    }

    void InputManager::initialize(GLFWwindow* window) {
        if (m_initialized) {
            LOG_WARNING("InputManager already initialized");
            return;
        }

        m_window = window;
        m_initialized = true;
        m_nextBindingId = 1;
        m_currentTime = 0.0f;
        m_mouseCaptured = false;
        m_mouseSensitivity = 1.0f;
        m_firstMouseMovement = true;
        m_scrollDelta = 0.0f;

        // Set GLFW callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPositionCallback);
        glfwSetScrollCallback(window, scrollCallback);

        // Get initial mouse position
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        m_mousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
        m_previousMousePosition = m_mousePosition;
        m_mouseDelta = glm::vec2(0.0f);

        LOG_INFO("InputManager initialized with GLFW window");
    }

    void InputManager::shutdown() {
        if (!m_initialized) {
            return;
        }

        // Clear callbacks
        if (m_window) {
            glfwSetKeyCallback(m_window, nullptr);
            glfwSetMouseButtonCallback(m_window, nullptr);
            glfwSetCursorPosCallback(m_window, nullptr);
            glfwSetScrollCallback(m_window, nullptr);
            glfwSetWindowUserPointer(m_window, nullptr);
        }

        // Clear all bindings
        clearAllBindings();

        m_window = nullptr;
        m_initialized = false;

        LOG_INFO("InputManager shutdown");
    }

    void InputManager::update(float deltaTime) {
        if (!m_initialized) {
            return;
        }

        m_currentTime += deltaTime;

        // Update previous states
        m_previousKeyStates = m_keyStates;
        m_previousMouseButtonStates = m_mouseButtonStates;

        // Reset per-frame values
        m_mouseDelta = glm::vec2(0.0f);
        m_scrollDelta = 0.0f;

        // Process held key bindings (for repeat actions)
        for (auto& [bindingId, binding] : m_bindings) {
            if (binding.actionType == InputActionType::HOLD) {
                bool isActive = false;
                
                if (!binding.isMouseAction) {
                    isActive = isKeyPressed(binding.key);
                } else {
                    isActive = isMouseButtonPressed(binding.mouseButton);
                }

                if (isActive) {
                    // Check repeat delay
                    if (m_currentTime - binding.lastTriggerTime >= binding.repeatDelay) {
                        InputEvent event;
                        event.type = InputEventType::KEY_REPEAT;
                        event.key = binding.key;
                        event.mouseButton = binding.mouseButton;
                        event.action = InputActionType::HOLD;
                        event.deltaTime = deltaTime;
                        event.mousePosition = m_mousePosition;
                        event.mouseDelta = m_mouseDelta;
                        event.scrollDelta = m_scrollDelta;
                        
                        // Populate additional data structures
                        event.mouseData.deltaX = m_mouseDelta.x;
                        event.mouseData.deltaY = m_mouseDelta.y;
                        event.scrollData.yOffset = m_scrollDelta;

                        binding.callback(event);
                        binding.lastTriggerTime = m_currentTime;
                    }
                }
            }
        }
    }

    int InputManager::registerBinding(const InputBinding& binding) {
        if (!m_initialized) {
            LOG_ERROR("InputManager not initialized");
            return -1;
        }

        int id = m_nextBindingId++;
        InputBinding newBinding = binding;
        newBinding.lastTriggerTime = 0.0f;
        
        m_bindings[id] = newBinding;
        
        LOG_INFO("Registered input binding '" + binding.actionName + "' with ID: " + std::to_string(id));
        return id;
    }

    void InputManager::unregisterBinding(int bindingId) {
        auto it = m_bindings.find(bindingId);
        if (it != m_bindings.end()) {
            LOG_INFO("Unregistered input binding '" + it->second.actionName + "' (ID: " + std::to_string(bindingId) + ")");
            m_bindings.erase(it);
        }
    }

    void InputManager::clearAllBindings() {
        size_t count = m_bindings.size();
        m_bindings.clear();
        LOG_INFO("Cleared all input bindings (" + std::to_string(count) + " bindings removed)");
    }

    bool InputManager::isKeyPressed(int key) const {
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second;
    }

    bool InputManager::isKeyJustPressed(int key) const {
        bool currentState = isKeyPressed(key);
        auto it = m_previousKeyStates.find(key);
        bool previousState = it != m_previousKeyStates.end() && it->second;
        return currentState && !previousState;
    }

    bool InputManager::isKeyJustReleased(int key) const {
        bool currentState = isKeyPressed(key);
        auto it = m_previousKeyStates.find(key);
        bool previousState = it != m_previousKeyStates.end() && it->second;
        return !currentState && previousState;
    }

    bool InputManager::isMouseButtonPressed(MouseButton button) const {
        auto it = m_mouseButtonStates.find(static_cast<int>(button));
        return it != m_mouseButtonStates.end() && it->second;
    }

    bool InputManager::isMouseButtonJustPressed(MouseButton button) const {
        bool currentState = isMouseButtonPressed(button);
        auto it = m_previousMouseButtonStates.find(static_cast<int>(button));
        bool previousState = it != m_previousMouseButtonStates.end() && it->second;
        return currentState && !previousState;
    }

    bool InputManager::isMouseButtonJustReleased(MouseButton button) const {
        bool currentState = isMouseButtonPressed(button);
        auto it = m_previousMouseButtonStates.find(static_cast<int>(button));
        bool previousState = it != m_previousMouseButtonStates.end() && it->second;
        return !currentState && previousState;
    }

    void InputManager::setCursorMode(int mode) {
        if (m_window) {
            glfwSetInputMode(m_window, GLFW_CURSOR, mode);
        }
    }

    int InputManager::getCursorMode() const {
        if (m_window) {
            return glfwGetInputMode(m_window, GLFW_CURSOR);
        }
        return GLFW_CURSOR_NORMAL;
    }

    void InputManager::setMouseCapture(bool enable) {
        m_mouseCaptured = enable;
        if (enable) {
            setCursorMode(GLFW_CURSOR_DISABLED);
        } else {
            setCursorMode(GLFW_CURSOR_NORMAL);
        }
        m_firstMouseMovement = true; // Reset to prevent jump
    }

    void InputManager::processKeyboardInput(int key, int action) {
        // Update key state
        if (action == GLFW_PRESS) {
            m_keyStates[key] = true;
            triggerKeyBindings(key, InputActionType::PRESS);
        } else if (action == GLFW_RELEASE) {
            m_keyStates[key] = false;
            triggerKeyBindings(key, InputActionType::RELEASE);
        } else if (action == GLFW_REPEAT) {
            triggerKeyBindings(key, InputActionType::REPEAT);
        }
    }

    void InputManager::processMouseButtonInput(int button, int action) {
        // Update mouse button state
        if (action == GLFW_PRESS) {
            m_mouseButtonStates[button] = true;
            triggerMouseBindings(static_cast<MouseButton>(button), InputActionType::PRESS);
        } else if (action == GLFW_RELEASE) {
            m_mouseButtonStates[button] = false;
            triggerMouseBindings(static_cast<MouseButton>(button), InputActionType::RELEASE);
        }
    }

    void InputManager::processMouseMovement(double xpos, double ypos) {
        glm::vec2 newPosition(static_cast<float>(xpos), static_cast<float>(ypos));

        if (m_firstMouseMovement) {
            m_previousMousePosition = newPosition;
            m_firstMouseMovement = false;
        }

        m_mouseDelta = (newPosition - m_previousMousePosition) * m_mouseSensitivity;
        m_previousMousePosition = m_mousePosition;
        m_mousePosition = newPosition;

        // Trigger mouse movement bindings if there's actual movement
        if (glm::length(m_mouseDelta) > 0.001f) {
            for (auto& [bindingId, binding] : m_bindings) {
                if (binding.actionName.find("MouseMovement") != std::string::npos ||
                    binding.actionName.find("Look") != std::string::npos ||
                    binding.actionName.find("SimpleMouseMove") != std::string::npos) {
                    
                    InputEvent event;
                    event.type = InputEventType::MOUSE_MOVE;
                    event.key = -1;
                    event.mouseButton = MouseButton::LEFT; // Dummy value
                    event.action = InputActionType::HOLD;
                    event.deltaTime = 0.0f; // Will be set by component
                    event.mousePosition = m_mousePosition;
                    event.mouseDelta = m_mouseDelta;
                    event.scrollDelta = 0.0f;
                    
                    // Populate additional data structures
                    event.mouseData.deltaX = m_mouseDelta.x;
                    event.mouseData.deltaY = m_mouseDelta.y;
                    event.scrollData.yOffset = 0.0f;

                    binding.callback(event);
                }
            }
        }
    }

    void InputManager::processMouseScroll(double xoffset, double yoffset) {
        m_scrollDelta = static_cast<float>(yoffset);

        // Trigger scroll bindings
        for (auto& [bindingId, binding] : m_bindings) {
            if (binding.actionName.find("Scroll") != std::string::npos ||
                binding.actionName.find("SimpleScroll") != std::string::npos) {
                InputEvent event;
                event.type = InputEventType::MOUSE_SCROLL;
                event.key = -1;
                event.mouseButton = MouseButton::LEFT; // Dummy value
                event.action = InputActionType::PRESS;
                event.deltaTime = 0.0f; // Will be set by component
                event.mousePosition = m_mousePosition;
                event.mouseDelta = glm::vec2(0.0f);
                event.scrollDelta = m_scrollDelta;
                
                // Populate additional data structures
                event.mouseData.deltaX = 0.0f;
                event.mouseData.deltaY = 0.0f;
                event.scrollData.yOffset = m_scrollDelta;

                binding.callback(event);
            }
        }
    }

    void InputManager::triggerKeyBindings(int key, InputActionType actionType) {
        for (auto& [bindingId, binding] : m_bindings) {
            if (!binding.isMouseAction && binding.key == key && binding.actionType == actionType) {
                InputEvent event;
                event.type = (actionType == InputActionType::PRESS) ? InputEventType::KEY_PRESS :
                           (actionType == InputActionType::RELEASE) ? InputEventType::KEY_RELEASE : InputEventType::KEY_REPEAT;
                event.key = key;
                event.mouseButton = MouseButton::LEFT; // Dummy value
                event.action = actionType;
                event.deltaTime = 0.0f; // Will be set by component
                event.mousePosition = m_mousePosition;
                event.mouseDelta = m_mouseDelta;
                event.scrollDelta = m_scrollDelta;
                
                // Populate additional data structures
                event.mouseData.deltaX = m_mouseDelta.x;
                event.mouseData.deltaY = m_mouseDelta.y;
                event.scrollData.yOffset = m_scrollDelta;

                binding.callback(event);
                binding.lastTriggerTime = m_currentTime;
            }
        }
    }

    void InputManager::triggerMouseBindings(MouseButton button, InputActionType actionType) {
        for (auto& [bindingId, binding] : m_bindings) {
            if (binding.isMouseAction && binding.mouseButton == button && binding.actionType == actionType) {
                InputEvent event;
                event.key = -1;
                event.mouseButton = button;
                event.action = actionType;
                event.deltaTime = 0.0f; // Will be set by component
                event.mousePosition = m_mousePosition;
                event.mouseDelta = m_mouseDelta;
                event.scrollDelta = m_scrollDelta;

                binding.callback(event);
                binding.lastTriggerTime = m_currentTime;
            }
        }
    }

    // GLFW callback functions
    void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (manager) {
            manager->processKeyboardInput(key, action);
        }
    }

    void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (manager) {
            manager->processMouseButtonInput(button, action);
        }
    }

    void InputManager::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
        InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (manager) {
            manager->processMouseMovement(xpos, ypos);
        }
    }

    void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (manager) {
            manager->processMouseScroll(xoffset, yoffset);
        }
    }

    // ============================================================================
    // InputComponent Implementation
    // ============================================================================

    InputComponent::InputComponent()
        : m_enabled(true)
        , m_initialized(false) {
    }

    InputComponent::InputComponent(const std::string& name)
        : m_enabled(true)
        , m_initialized(false) {
        // Store name for debugging (if needed later)
        (void)name; // Suppress unused parameter warning for now
    }

    InputComponent::~InputComponent() {
        cleanup();
    }

    InputComponent::InputComponent(const InputComponent& other)
        : m_enabled(other.m_enabled)
        , m_initialized(false) {
        // Note: We don't copy bindings in copy constructor to avoid issues
        // Bindings should be set up after construction
    }

    InputComponent::InputComponent(InputComponent&& other) noexcept
        : m_bindingIds(std::move(other.m_bindingIds))
        , m_actionNames(std::move(other.m_actionNames))
        , m_enabled(other.m_enabled)
        , m_initialized(other.m_initialized) {
        
        // Clear moved-from object
        other.m_bindingIds.clear();
        other.m_actionNames.clear();
        other.m_initialized = false;
    }

    InputComponent& InputComponent::operator=(const InputComponent& other) {
        if (this != &other) {
            cleanup();
            m_enabled = other.m_enabled;
            m_initialized = false;
            // Note: We don't copy bindings in assignment to avoid issues
        }
        return *this;
    }

    InputComponent& InputComponent::operator=(InputComponent&& other) noexcept {
        if (this != &other) {
            cleanup();
            
            m_bindingIds = std::move(other.m_bindingIds);
            m_actionNames = std::move(other.m_actionNames);
            m_enabled = other.m_enabled;
            m_initialized = other.m_initialized;
            
            // Clear moved-from object
            other.m_bindingIds.clear();
            other.m_actionNames.clear();
            other.m_initialized = false;
        }
        return *this;
    }

    void InputComponent::initialize() {
        if (m_initialized) {
            LOG_WARNING("InputComponent already initialized");
            return;
        }

        m_initialized = true;
        LOG_INFO("InputComponent initialized");
    }

    void InputComponent::update(float deltaTime) {
        if (!m_initialized || !m_enabled) {
            return;
        }

        // InputComponent doesn't need per-frame updates by default
        // All input processing is handled by callbacks through InputManager
    }

    void InputComponent::cleanup() {
        if (!m_initialized) {
            return;
        }

        clearAllBindings();
        m_initialized = false;
        LOG_INFO("InputComponent cleaned up");
    }

    int InputComponent::bindKeyAction(const std::string& actionName, int key, InputActionType actionType,
                                     InputCallback callback, float repeatDelay) {
        if (!m_initialized) {
            LOG_ERROR("InputComponent not initialized");
            return -1;
        }

        InputBinding binding;
        binding.actionName = actionName;
        binding.key = key;
        binding.mouseButton = MouseButton::LEFT; // Dummy value
        binding.actionType = actionType;
        binding.callback = callback;
        binding.isMouseAction = false;
        binding.repeatDelay = repeatDelay;
        binding.lastTriggerTime = 0.0f;

        int bindingId = getInputManager().registerBinding(binding);
        if (bindingId != -1) {
            m_bindingIds.push_back(bindingId);
            m_actionNames.push_back(actionName);
        }

        return bindingId;
    }

    int InputComponent::bindMouseAction(const std::string& actionName, MouseButton button, InputActionType actionType,
                                       InputCallback callback) {
        if (!m_initialized) {
            LOG_ERROR("InputComponent not initialized");
            return -1;
        }

        InputBinding binding;
        binding.actionName = actionName;
        binding.key = -1; // Dummy value
        binding.mouseButton = button;
        binding.actionType = actionType;
        binding.callback = callback;
        binding.isMouseAction = true;
        binding.repeatDelay = 0.0f;
        binding.lastTriggerTime = 0.0f;

        int bindingId = getInputManager().registerBinding(binding);
        if (bindingId != -1) {
            m_bindingIds.push_back(bindingId);
            m_actionNames.push_back(actionName);
        }

        return bindingId;
    }

    int InputComponent::bindMouseMovement(const std::string& actionName, InputCallback callback) {
        if (!m_initialized) {
            LOG_ERROR("InputComponent not initialized");
            return -1;
        }

        InputBinding binding;
        binding.actionName = actionName + "_MouseMovement";
        binding.key = -1;
        binding.mouseButton = MouseButton::LEFT;
        binding.actionType = InputActionType::HOLD;
        binding.callback = callback;
        binding.isMouseAction = true;
        binding.repeatDelay = 0.0f;
        binding.lastTriggerTime = 0.0f;

        int bindingId = getInputManager().registerBinding(binding);
        if (bindingId != -1) {
            m_bindingIds.push_back(bindingId);
            m_actionNames.push_back(actionName);
        }

        return bindingId;
    }

    int InputComponent::bindMouseScroll(const std::string& actionName, InputCallback callback) {
        if (!m_initialized) {
            LOG_ERROR("InputComponent not initialized");
            return -1;
        }

        InputBinding binding;
        binding.actionName = actionName + "_Scroll";
        binding.key = -1;
        binding.mouseButton = MouseButton::LEFT;
        binding.actionType = InputActionType::PRESS;
        binding.callback = callback;
        binding.isMouseAction = true;
        binding.repeatDelay = 0.0f;
        binding.lastTriggerTime = 0.0f;

        int bindingId = getInputManager().registerBinding(binding);
        if (bindingId != -1) {
            m_bindingIds.push_back(bindingId);
            m_actionNames.push_back(actionName);
        }

        return bindingId;
    }

    void InputComponent::unbindAction(int bindingId) {
        auto it = std::find(m_bindingIds.begin(), m_bindingIds.end(), bindingId);
        if (it != m_bindingIds.end()) {
            size_t index = std::distance(m_bindingIds.begin(), it);
            
            getInputManager().unregisterBinding(bindingId);
            m_bindingIds.erase(it);
            
            if (index < m_actionNames.size()) {
                m_actionNames.erase(m_actionNames.begin() + index);
            }
        }
    }

    void InputComponent::unbindAction(const std::string& actionName) {
        for (size_t i = 0; i < m_actionNames.size(); ) {
            if (m_actionNames[i] == actionName || m_actionNames[i].find(actionName) != std::string::npos) {
                getInputManager().unregisterBinding(m_bindingIds[i]);
                m_bindingIds.erase(m_bindingIds.begin() + i);
                m_actionNames.erase(m_actionNames.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void InputComponent::clearAllBindings() {
        for (int bindingId : m_bindingIds) {
            getInputManager().unregisterBinding(bindingId);
        }
        m_bindingIds.clear();
        m_actionNames.clear();
    }

    int InputComponent::bindKeyAction(int key, InputCallback callback) {
        // Register for all action types and return the first binding ID
        bindKeyAction("SimpleKey_Press_" + std::to_string(key), key, InputActionType::PRESS, callback);
        bindKeyAction("SimpleKey_Release_" + std::to_string(key), key, InputActionType::RELEASE, callback);
        bindKeyAction("SimpleKey_Repeat_" + std::to_string(key), key, InputActionType::REPEAT, callback);
        return m_bindingIds.back(); // Return the last registered ID
    }

    int InputComponent::bindMouseMoveAction(InputCallback callback) {
        return bindMouseMovement("SimpleMouseMove", callback);
    }

    int InputComponent::bindScrollAction(InputCallback callback) {
        return bindMouseScroll("SimpleScroll", callback);
    }

    void InputComponent::copyBindingsFrom(const InputComponent& other) {
        // This would be complex to implement correctly, so we leave it empty
        // Bindings should be set up after construction anyway
    }

} // namespace IKore
