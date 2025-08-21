#include "LODComponent.h"
#include "../Entity.h"
#include "../components/TransformComponent.h"
#include "../Logger.h"
#include <algorithm>
#include <chrono>

namespace IKore {

LODComponent::LODComponent() 
    : m_currentLOD(LODLevel::LOD_0)
    , m_previousLOD(LODLevel::LOD_0)
    , m_enabled(true)
    , m_forcedLOD(false)
    , m_initialized(false)
    , m_distanceToCamera(0.0f)
    , m_hysteresis(0.1f)
    , m_transitionDuration(0.5f)
    , m_transitionTimer(0.0f)
    , m_inTransition(false)
    , m_lodSwitchCount(0)
    , m_totalUpdateTime(0.0f)
    , m_updateCount(0)
{
    // Initialize LOD configurations array (5 levels)
    m_lodConfigs.resize(5);
    m_lodModels.resize(5);
    
    LOG_INFO("LODComponent created");
}

LODComponent::~LODComponent() {
    LOG_INFO("LODComponent destroyed");
}

void LODComponent::onAttach() {
    LOG_DEBUG("LODComponent attached to entity");
}

void LODComponent::onDetach() {
    LOG_DEBUG("LODComponent detached from entity");
}

bool LODComponent::initialize() {
    if (m_initialized) {
        LOG_WARNING("LODComponent already initialized");
        return true;
    }
    
    // Load all configured LOD models
    bool anyLoaded = false;
    for (int i = 0; i < 5; ++i) {
        LODLevel level = static_cast<LODLevel>(i);
        if (m_lodConfigs[i].enabled && !m_lodConfigs[i].modelPath.empty()) {
            if (loadLODModel(level)) {
                anyLoaded = true;
                std::string logMsg = "Loaded LOD level " + std::to_string(i) + " model: " + m_lodConfigs[i].modelPath;
                LOG_INFO(logMsg);
            } else {
                std::string logMsg = "Failed to load LOD level " + std::to_string(i) + " model: " + m_lodConfigs[i].modelPath;
                LOG_WARNING(logMsg);
            }
        }
    }
    
    if (!anyLoaded) {
        LOG_ERROR("No LOD models could be loaded");
        return false;
    }
    
    m_initialized = true;
    LOG_INFO("LODComponent initialized successfully");
    return true;
}

void LODComponent::update(const glm::vec3& cameraPosition, float deltaTime) {
    if (!m_enabled || !m_initialized) {
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Calculate distance to camera
    m_distanceToCamera = calculateDistanceToCamera(cameraPosition);
    
    // Update transition if active
    if (m_inTransition) {
        updateTransition(deltaTime);
    }
    
    // Determine optimal LOD level (unless forced)
    if (!m_forcedLOD && !m_inTransition) {
        LODLevel optimalLOD = determineLODLevel(m_distanceToCamera);
        
        // Switch if needed (with hysteresis to prevent flickering)
        if (optimalLOD != m_currentLOD) {
            float hysteresisDistance = m_distanceToCamera * m_hysteresis;
            
            // Check if we should really switch by applying hysteresis
            bool shouldSwitch = false;
            if (optimalLOD > m_currentLOD) { // Moving to lower detail
                shouldSwitch = (m_distanceToCamera > m_lodConfigs[static_cast<int>(optimalLOD)].distanceThreshold + hysteresisDistance);
            } else { // Moving to higher detail
                shouldSwitch = (m_distanceToCamera < m_lodConfigs[static_cast<int>(optimalLOD)].distanceThreshold - hysteresisDistance);
            }
            
            if (shouldSwitch) {
                switchToLOD(optimalLOD);
            }
        }
    }
    
    // Update performance tracking
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_totalUpdateTime += duration.count() / 1000.0f; // Convert to milliseconds
    m_updateCount++;
}

void LODComponent::addLODLevel(LODLevel level, float distanceThreshold, const std::string& modelPath, int triangleCount) {
    int index = static_cast<int>(level);
    if (index < 0 || index >= 5) {
        std::string logMsg = "Invalid LOD level: " + std::to_string(index);
        LOG_ERROR(logMsg);
        return;
    }
    
    m_lodConfigs[index] = LODConfig(distanceThreshold, modelPath, triangleCount);
    
    std::string logMsg = "Added LOD level " + std::to_string(index) + " at distance " + std::to_string(distanceThreshold) + " with model: " + modelPath;
    LOG_INFO(logMsg);
    
    // If already initialized, load the model immediately
    if (m_initialized) {
        loadLODModel(level);
    }
}

void LODComponent::removeLODLevel(LODLevel level) {
    int index = static_cast<int>(level);
    if (index < 0 || index >= 5) {
        return;
    }
    
    m_lodConfigs[index].enabled = false;
    m_lodConfigs[index].modelPath.clear();
    m_lodModels[index].reset();
    
    std::string logMsg = "Removed LOD level " + std::to_string(index);
    LOG_INFO(logMsg);
}

std::shared_ptr<Model> LODComponent::getCurrentModel() const {
    int index = static_cast<int>(m_currentLOD);
    if (index >= 0 && index < 5) {
        return m_lodModels[index];
    }
    return nullptr;
}

void LODComponent::setHysteresis(float factor) {
    m_hysteresis = std::clamp(factor, 0.0f, 1.0f);
    std::string logMsg = "LOD hysteresis set to " + std::to_string(m_hysteresis);
    LOG_DEBUG(logMsg);
}

void LODComponent::forceLODLevel(LODLevel level) {
    if (!hasLODLevel(level)) {
        std::string logMsg = "Cannot force LOD level " + std::to_string(static_cast<int>(level)) + " - not available";
        LOG_WARNING(logMsg);
        return;
    }
    
    m_forcedLOD = true;
    switchToLOD(level);
    std::string logMsg = "Forced LOD level to " + std::to_string(static_cast<int>(level));
    LOG_INFO(logMsg);
}

const LODConfig& LODComponent::getLODConfig(LODLevel level) const {
    int index = static_cast<int>(level);
    if (index >= 0 && index < 5) {
        return m_lodConfigs[index];
    }
    
    // Return default config for invalid levels
    static LODConfig defaultConfig;
    return defaultConfig;
}

void LODComponent::setLODConfig(LODLevel level, const LODConfig& config) {
    int index = static_cast<int>(level);
    if (index >= 0 && index < 5) {
        m_lodConfigs[index] = config;
        
        // Reload model if component is initialized
        if (m_initialized && config.enabled && !config.modelPath.empty()) {
            loadLODModel(level);
        }
    }
}

std::string LODComponent::getPerformanceStats() const {
    std::string stats = "LOD Performance Stats:\n";
    stats += "- Current LOD: " + std::to_string(static_cast<int>(m_currentLOD)) + "\n";
    stats += "- Distance to Camera: " + std::to_string(m_distanceToCamera) + "\n";
    stats += "- LOD Switches: " + std::to_string(m_lodSwitchCount) + "\n";
    stats += "- Update Count: " + std::to_string(m_updateCount) + "\n";
    
    if (m_updateCount > 0) {
        float avgUpdateTime = m_totalUpdateTime / m_updateCount;
        stats += "- Avg Update Time: " + std::to_string(avgUpdateTime) + " ms\n";
    }
    
    stats += "- Triangle Count: " + std::to_string(getCurrentTriangleCount()) + "\n";
    stats += "- In Transition: " + std::string(m_inTransition ? "Yes" : "No") + "\n";
    
    return stats;
}

void LODComponent::resetPerformanceStats() {
    m_lodSwitchCount = 0;
    m_totalUpdateTime = 0.0f;
    m_updateCount = 0;
    LOG_DEBUG("LOD performance stats reset");
}

int LODComponent::getCurrentTriangleCount() const {
    int index = static_cast<int>(m_currentLOD);
    if (index >= 0 && index < 5) {
        return m_lodConfigs[index].triangleCount;
    }
    return 0;
}

bool LODComponent::hasLODLevel(LODLevel level) const {
    int index = static_cast<int>(level);
    if (index >= 0 && index < 5) {
        return m_lodConfigs[index].enabled && m_lodModels[index] != nullptr;
    }
    return false;
}

float LODComponent::calculateDistanceToCamera(const glm::vec3& cameraPosition) const {
    glm::vec3 entityPos = getEntityPosition();
    return glm::length(cameraPosition - entityPos);
}

LODLevel LODComponent::determineLODLevel(float distance) const {
    // Find the highest detail LOD level that matches the distance
    LODLevel bestLOD = LODLevel::LOD_0;
    
    for (int i = 0; i < 5; ++i) {
        LODLevel level = static_cast<LODLevel>(i);
        if (m_lodConfigs[i].enabled && distance >= m_lodConfigs[i].distanceThreshold) {
            bestLOD = level;
        }
    }
    
    return bestLOD;
}

bool LODComponent::loadLODModel(LODLevel level) {
    int index = static_cast<int>(level);
    if (index < 0 || index >= 5) {
        return false;
    }
    
    const LODConfig& config = m_lodConfigs[index];
    if (!config.enabled || config.modelPath.empty()) {
        return false;
    }
    
    try {
        auto model = std::make_shared<Model>();
        if (model->loadFromFile(config.modelPath)) {
            m_lodModels[index] = model;
            std::string logMsg = "Loaded LOD " + std::to_string(index) + " model: " + config.modelPath;
            LOG_INFO(logMsg);
            return true;
        } else {
            std::string logMsg = "Failed to load LOD " + std::to_string(index) + " model: " + config.modelPath;
            LOG_ERROR(logMsg);
            m_lodModels[index].reset();
            return false;
        }
    } catch (const std::exception& e) {
        std::string logMsg = "Failed to load LOD " + std::to_string(index) + " model '" + config.modelPath + "': " + e.what();
        LOG_ERROR(logMsg);
        m_lodModels[index].reset();
        return false;
    }
}

void LODComponent::switchToLOD(LODLevel newLOD) {
    if (newLOD == m_currentLOD) {
        return;
    }
    
    if (!hasLODLevel(newLOD)) {
        std::string logMsg = "Cannot switch to LOD level " + std::to_string(static_cast<int>(newLOD)) + " - not available";
        LOG_WARNING(logMsg);
        return;
    }
    
    m_previousLOD = m_currentLOD;
    m_currentLOD = newLOD;
    m_lodSwitchCount++;
    
    // Start transition if duration is set
    if (m_transitionDuration > 0.0f) {
        m_inTransition = true;
        m_transitionTimer = 0.0f;
    }
    
    std::string logMsg = "Switched from LOD " + std::to_string(static_cast<int>(m_previousLOD)) + 
                        " to LOD " + std::to_string(static_cast<int>(m_currentLOD)) +
                        " (distance: " + std::to_string(m_distanceToCamera) + ")";
    LOG_DEBUG(logMsg);
}

void LODComponent::updateTransition(float deltaTime) {
    if (!m_inTransition) {
        return;
    }
    
    m_transitionTimer += deltaTime;
    
    if (m_transitionTimer >= m_transitionDuration) {
        m_inTransition = false;
        m_transitionTimer = 0.0f;
        LOG_DEBUG("LOD transition completed");
    }
}

glm::vec3 LODComponent::getEntityPosition() const {
    auto entity = getEntity().lock();
    if (entity) {
        auto transform = entity->getComponent<TransformComponent>();
        if (transform) {
            return transform->getPosition();
        }
    }
    return glm::vec3(0.0f);
}

} // namespace IKore
