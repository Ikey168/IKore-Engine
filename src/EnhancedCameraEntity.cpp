#include "EnhancedCameraEntity.h"
#include "Logger.h"
#include <sstream>

namespace IKore {

    // ============================================================================
    // Constructors
    // ============================================================================

    EnhancedCameraEntity::EnhancedCameraEntity(const std::string& name,
                              const glm::vec3& position,
                              const glm::vec3& target,
                              CameraComponent::CameraType cameraType)
        : m_name(name)
        , m_cameraComponent(position, target)
        , m_active(true) {
        
        m_cameraComponent.setCameraType(cameraType);
        LOG_INFO("EnhancedCameraEntity '" + m_name + "' created with ID: " + std::to_string(getID()) +
                 " at position (" + std::to_string(position.x) + ", " + 
                 std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
    }

    EnhancedCameraEntity::EnhancedCameraEntity(const std::string& name,
                              const glm::vec3& position,
                              const glm::vec3& target,
                              const glm::vec3& up,
                              float fov,
                              float aspectRatio,
                              float nearPlane,
                              float farPlane,
                              CameraComponent::CameraType cameraType)
        : m_name(name)
        , m_cameraComponent(position, target, up, fov, aspectRatio, nearPlane, farPlane)
        , m_active(true) {
        
        m_cameraComponent.setCameraType(cameraType);
        LOG_INFO("EnhancedCameraEntity '" + m_name + "' created with full parameters and ID: " + std::to_string(getID()));
    }

    // ============================================================================
    // Entity Overrides
    // ============================================================================

    void EnhancedCameraEntity::initialize() {
        LOG_INFO("EnhancedCameraEntity '" + m_name + "' initialized (FOV: " + 
                 std::to_string(m_cameraComponent.getFieldOfView()) + 
                 ", Aspect: " + std::to_string(m_cameraComponent.getAspectRatio()) + 
                 ", Type: " + std::to_string(static_cast<int>(m_cameraComponent.getCameraType())) + ")");
    }

    void EnhancedCameraEntity::update(float deltaTime) {
        if (!m_active) {
            return;
        }

        // Update the camera component
        m_cameraComponent.update(deltaTime);
    }

    void EnhancedCameraEntity::cleanup() {
        LOG_INFO("EnhancedCameraEntity '" + m_name + "' cleaned up");
    }

    glm::vec3 EnhancedCameraEntity::getPosition() const {
        return m_cameraComponent.getPosition();
    }

    // ============================================================================
    // Debug and Information
    // ============================================================================

    std::string EnhancedCameraEntity::getDebugInfo() const {
        std::stringstream ss;
        ss << "EnhancedCameraEntity '" << m_name << "' (ID: " << getID() << "):\n";
        ss << "  Active: " << (m_active ? "Yes" : "No") << "\n";
        ss << "  Position: (" << getPosition().x << ", " << getPosition().y << ", " << getPosition().z << ")\n";
        ss << "  Target: (" << getTarget().x << ", " << getTarget().y << ", " << getTarget().z << ")\n";
        ss << "  FOV: " << getFieldOfView() << " degrees\n";
        ss << "  Aspect Ratio: " << getAspectRatio() << "\n";
        ss << "  Near/Far Planes: " << getNearPlane() << " / " << getFarPlane() << "\n";
        
        const char* typeNames[] = { "First-Person", "Third-Person", "Orbital", "Free Camera", "Static" };
        int typeIndex = static_cast<int>(getCameraType());
        if (typeIndex >= 0 && typeIndex < 5) {
            ss << "  Camera Type: " << typeNames[typeIndex] << "\n";
        } else {
            ss << "  Camera Type: Unknown (" << typeIndex << ")\n";
        }
        
        if (isFollowingEntity()) {
            auto target = getFollowTarget();
            if (target) {
                ss << "  Following Entity: ID " << target->getID() << "\n";
            } else {
                ss << "  Following Entity: (expired reference)\n";
            }
        } else {
            ss << "  Following Entity: None\n";
        }
        
        if (isTransitioning()) {
            ss << "  Transitioning: " << (getTransitionProgress() * 100.0f) << "% complete\n";
        }
        
        return ss.str();
    }

} // namespace IKore
