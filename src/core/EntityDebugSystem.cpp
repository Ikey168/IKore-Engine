#include "core/EntityDebugSystem.h"
#include "core/EntityTypes.h"
#include "core/Logger.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace IKore {

    EntityDebugSystem& EntityDebugSystem::getInstance() {
        static EntityDebugSystem instance;
        return instance;
    }

    void EntityDebugSystem::toggleDebugMode() {
        m_debugEnabled = !m_debugEnabled;
        if (m_debugEnabled) {
            LOG_INFO("Entity Debug System: Enabled");
        } else {
            LOG_INFO("Entity Debug System: Disabled");
        }
    }

    void EntityDebugSystem::enableDebugMode() {
        if (!m_debugEnabled) {
            m_debugEnabled = true;
            LOG_INFO("Entity Debug System: Enabled");
        }
    }

    void EntityDebugSystem::disableDebugMode() {
        if (m_debugEnabled) {
            m_debugEnabled = false;
            LOG_INFO("Entity Debug System: Disabled");
        }
    }

    void EntityDebugSystem::update(float deltaTime) {
        if (!m_debugEnabled) {
            return;
        }

        float debugStartTime = getCurrentTimeMs();

        // Collect debug information from all entities
        collectEntityDebugInfo(deltaTime);

        // Update performance metrics if monitoring is enabled
        if (m_performanceMonitoring) {
            float debugEndTime = getCurrentTimeMs();
            float debugTime = debugEndTime - debugStartTime;
            updatePerformanceMetrics(deltaTime, debugTime);
        }
    }

    void EntityDebugSystem::renderDebugOverlay() {
        if (!m_debugEnabled || m_entityDebugInfo.empty()) {
            return;
        }

        // Calculate overlay dimensions
        float lineHeight = 0.02f * m_textSize;
        size_t displayCount = std::min(m_entityDebugInfo.size(), m_maxDisplayEntities);
        
        // Calculate lines per entity (name + basic info + components)
        size_t totalLines = 2; // Header
        for (size_t i = 0; i < displayCount; ++i) {
            totalLines += 3; // Entity header, position/rotation, scale/status
            if (m_showDetailedComponents) {
                totalLines += m_entityDebugInfo[i].componentDetails.size();
            }
            totalLines += 1; // Separator line
        }
        
        if (m_performanceMonitoring) {
            totalLines += 6; // Performance metrics
        }

        float overlayHeight = totalLines * lineHeight + 0.02f; // Add padding
        float overlayWidth = 0.35f; // Fixed width

        // Render background
        renderDebugBackground(m_overlayPosition.x - 0.01f, m_overlayPosition.y - 0.01f, 
                            overlayWidth + 0.02f, overlayHeight + 0.02f);

        float currentY = m_overlayPosition.y;

        // Render header
        std::ostringstream headerStream;
        headerStream << "=== Entity Debug System ===";
        renderDebugText(headerStream.str(), m_overlayPosition.x, currentY, m_textColor);
        currentY += lineHeight;

        headerStream.str("");
        headerStream << "Entities: " << m_entityDebugInfo.size() 
                    << " (showing " << displayCount << ")";
        renderDebugText(headerStream.str(), m_overlayPosition.x, currentY, m_textColor);
        currentY += lineHeight * 1.5f;

        // Render entity information
        for (size_t i = 0; i < displayCount; ++i) {
            const EntityDebugInfo& info = m_entityDebugInfo[i];

            // Entity header
            std::ostringstream entityStream;
            entityStream << "[" << info.entityID << "] " << info.name 
                        << " (" << info.type << ")";
            renderDebugText(entityStream.str(), m_overlayPosition.x, currentY, 
                          glm::vec4(0.8f, 1.0f, 0.8f, 1.0f)); // Light green
            currentY += lineHeight;

            // Position and rotation
            std::ostringstream posRotStream;
            posRotStream << "  Pos: (" << std::fixed << std::setprecision(2) 
                        << info.position.x << ", " << info.position.y << ", " << info.position.z << ")";
            posRotStream << "  Rot: (" << info.rotation.x << ", " << info.rotation.y << ", " << info.rotation.z << ")";
            renderDebugText(posRotStream.str(), m_overlayPosition.x, currentY, m_textColor);
            currentY += lineHeight;

            // Scale and status
            std::ostringstream scaleStatusStream;
            scaleStatusStream << "  Scale: (" << info.scale.x << ", " << info.scale.y << ", " << info.scale.z << ")";
            scaleStatusStream << "  Active: " << (info.isActive ? "Yes" : "No") 
                             << "  Visible: " << (info.isVisible ? "Yes" : "No");
            renderDebugText(scaleStatusStream.str(), m_overlayPosition.x, currentY, m_textColor);
            currentY += lineHeight;

            // Component details (if enabled)
            if (m_showDetailedComponents && !info.componentDetails.empty()) {
                for (const std::string& detail : info.componentDetails) {
                    std::string componentLine = "    " + detail;
                    renderDebugText(componentLine, m_overlayPosition.x, currentY, 
                                  glm::vec4(0.8f, 0.8f, 1.0f, 1.0f)); // Light blue
                    currentY += lineHeight;
                }
            }

            currentY += lineHeight * 0.5f; // Separator
        }

        // Render performance metrics (if enabled)
        if (m_performanceMonitoring) {
            currentY += lineHeight * 0.5f;
            
            renderDebugText("=== Performance Metrics ===", m_overlayPosition.x, currentY, 
                          glm::vec4(1.0f, 1.0f, 0.8f, 1.0f)); // Light yellow
            currentY += lineHeight;

            std::ostringstream perfStream;
            perfStream << "Debug Overhead: " << std::fixed << std::setprecision(3) 
                      << m_performanceMetrics.debugOverheadMs << " ms";
            renderDebugText(perfStream.str(), m_overlayPosition.x, currentY, m_textColor);
            currentY += lineHeight;

            perfStream.str("");
            perfStream << "Avg Frame Time: " << m_performanceMetrics.averageFrameTime << " ms";
            renderDebugText(perfStream.str(), m_overlayPosition.x, currentY, m_textColor);
            currentY += lineHeight;

            perfStream.str("");
            perfStream << "Min/Max Frame: " << m_performanceMetrics.minFrameTime 
                      << " / " << m_performanceMetrics.maxFrameTime << " ms";
            renderDebugText(perfStream.str(), m_overlayPosition.x, currentY, m_textColor);
            currentY += lineHeight;

            perfStream.str("");
            perfStream << "Total Entities: " << m_performanceMetrics.totalEntities 
                      << "  Visible: " << m_performanceMetrics.visibleEntities;
            renderDebugText(perfStream.str(), m_overlayPosition.x, currentY, m_textColor);
        }
    }

    void EntityDebugSystem::setOverlayPosition(float x, float y) {
        m_overlayPosition = glm::vec2(std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f));
    }

    void EntityDebugSystem::setEntityFilter(std::function<bool(const std::shared_ptr<Entity>&)> filter) {
        m_entityFilter = filter;
    }

    void EntityDebugSystem::collectEntityDebugInfo(float deltaTime) {
        m_entityDebugInfo.clear();

        EntityManager& entityManager = EntityManager::getInstance();
        
        // Iterate through all entities and collect debug information
        entityManager.forEach([this, deltaTime](std::shared_ptr<Entity> entity) {
            // Apply filter if set
            if (m_entityFilter && !m_entityFilter(entity)) {
                return;
            }

            EntityDebugInfo info = extractEntityInfo(entity);
            info.frameTime = deltaTime * 1000.0f; // Convert to milliseconds
            m_entityDebugInfo.push_back(info);
        });

        // Sort entities by ID for consistent display order
        std::sort(m_entityDebugInfo.begin(), m_entityDebugInfo.end(),
                 [](const EntityDebugInfo& a, const EntityDebugInfo& b) {
                     return a.entityID < b.entityID;
                 });
    }

    EntityDebugInfo EntityDebugSystem::extractEntityInfo(const std::shared_ptr<Entity>& entity) {
        EntityDebugInfo info;
        info.entityID = entity->getID();

        // Try to cast to known entity types to get more detailed information
        if (auto gameObject = std::dynamic_pointer_cast<GameObject>(entity)) {
            info.name = gameObject->getName();
            info.position = gameObject->getPosition();
            info.rotation = gameObject->getRotation();
            info.scale = gameObject->getScale();
            info.isActive = gameObject->isActive();
            info.isVisible = gameObject->isVisible();

            // Determine specific type
            if (std::dynamic_pointer_cast<LightEntity>(entity)) {
                info.type = "LightEntity";
            } else if (std::dynamic_pointer_cast<CameraEntity>(entity)) {
                info.type = "CameraEntity";
            } else if (std::dynamic_pointer_cast<TestEntity>(entity)) {
                info.type = "TestEntity";
            } else {
                info.type = "GameObject";
            }

            info.componentDetails = getComponentDetails(entity);
        } else {
            // Fallback for base Entity types
            info.name = "Entity_" + std::to_string(info.entityID);
            info.type = "Entity";
            info.position = glm::vec3(0.0f);
            info.rotation = glm::vec3(0.0f);
            info.scale = glm::vec3(1.0f);
            info.isActive = true;
            info.isVisible = true;
            info.componentDetails = {"Base Entity (no GameObject data)"};
        }

        return info;
    }

    std::vector<std::string> EntityDebugSystem::getComponentDetails(const std::shared_ptr<Entity>& entity) {
        std::vector<std::string> details;

        // Check for LightEntity-specific components
        if (auto lightEntity = std::dynamic_pointer_cast<LightEntity>(entity)) {
            std::ostringstream lightStream;
            lightStream << "Light Type: ";
            switch (lightEntity->getLightType()) {
                case LightEntity::LightType::DIRECTIONAL:
                    lightStream << "Directional";
                    break;
                case LightEntity::LightType::POINT:
                    lightStream << "Point";
                    break;
                case LightEntity::LightType::SPOT:
                    lightStream << "Spot";
                    break;
            }
            details.push_back(lightStream.str());

            lightStream.str("");
            const glm::vec3& color = lightEntity->getColor();
            lightStream << "Color: (" << std::fixed << std::setprecision(2) 
                       << color.r << ", " << color.g << ", " << color.b << ")";
            details.push_back(lightStream.str());

            lightStream.str("");
            lightStream << "Intensity: " << lightEntity->getIntensity() 
                       << "  Range: " << lightEntity->getRange();
            details.push_back(lightStream.str());

            if (lightEntity->getLightType() == LightEntity::LightType::SPOT) {
                lightStream.str("");
                lightStream << "Spot Angle: " << lightEntity->getSpotAngle() << "°";
                details.push_back(lightStream.str());
            }
        }

        // Check for CameraEntity-specific components
        else if (auto cameraEntity = std::dynamic_pointer_cast<CameraEntity>(entity)) {
            std::ostringstream cameraStream;
            cameraStream << "FOV: " << cameraEntity->getFieldOfView() << "°";
            details.push_back(cameraStream.str());

            cameraStream.str("");
            cameraStream << "Aspect: " << std::fixed << std::setprecision(3) 
                        << cameraEntity->getAspectRatio();
            details.push_back(cameraStream.str());

            cameraStream.str("");
            cameraStream << "Near: " << cameraEntity->getNearPlane() 
                        << "  Far: " << cameraEntity->getFarPlane();
            details.push_back(cameraStream.str());
        }

        // Check for TestEntity-specific components
        else if (auto testEntity = std::dynamic_pointer_cast<TestEntity>(entity)) {
            std::ostringstream testStream;
            testStream << "Lifetime: " << std::fixed << std::setprecision(2) 
                      << testEntity->getLifetime() << "s";
            details.push_back(testStream.str());

            testStream.str("");
            testStream << "Age: " << testEntity->getAge() << "s";
            details.push_back(testStream.str());

            testStream.str("");
            testStream << "Expires: " << (testEntity->isExpired() ? "Yes" : "No");
            details.push_back(testStream.str());
        }

        // If no specific components found, add basic GameObject info
        if (details.empty() && std::dynamic_pointer_cast<GameObject>(entity)) {
            details.push_back("Transform Component: Position, Rotation, Scale");
            details.push_back("Visibility Component: Active, Visible flags");
        }

        return details;
    }

    void EntityDebugSystem::updatePerformanceMetrics(float deltaTime, float debugTime) {
        m_performanceMetrics.debugOverheadMs = debugTime;
        m_performanceMetrics.totalEntities = m_entityDebugInfo.size();
        
        // Count visible entities
        m_performanceMetrics.visibleEntities = std::count_if(
            m_entityDebugInfo.begin(), m_entityDebugInfo.end(),
            [](const EntityDebugInfo& info) { return info.isVisible; }
        );

        // Update frame time statistics
        float frameTimeMs = deltaTime * 1000.0f;
        m_frameTimeAccumulator += frameTimeMs;
        m_frameCount++;

        if (m_frameCount == 1) {
            m_performanceMetrics.minFrameTime = frameTimeMs;
            m_performanceMetrics.maxFrameTime = frameTimeMs;
        } else {
            m_performanceMetrics.minFrameTime = std::min(m_performanceMetrics.minFrameTime, frameTimeMs);
            m_performanceMetrics.maxFrameTime = std::max(m_performanceMetrics.maxFrameTime, frameTimeMs);
        }

        // Calculate average every 60 frames
        if (m_frameCount >= 60) {
            m_performanceMetrics.averageFrameTime = m_frameTimeAccumulator / m_frameCount;
            m_frameTimeAccumulator = 0.0f;
            m_frameCount = 0;
        }
    }

    void EntityDebugSystem::renderDebugText(const std::string& text, [[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] const glm::vec4& color) {
        // This is a placeholder implementation
        // In a real implementation, you would use your rendering system to draw text
        // For now, we'll just log the text if needed
        static bool firstCall = true;
        if (firstCall) {
            LOG_INFO("EntityDebugSystem: Text rendering not implemented yet. Would display: " + text);
            firstCall = false;
        }
    }

    void EntityDebugSystem::renderDebugBackground([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float width, [[maybe_unused]] float height) {
        // This is a placeholder implementation
        // In a real implementation, you would render a semi-transparent background quad
        // This keeps the interface ready for when rendering is implemented
    }

    float EntityDebugSystem::getCurrentTimeMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        return static_cast<float>(millis) / 1000.0f;
    }

    EntityDebugSystem& getEntityDebugSystem() {
        return EntityDebugSystem::getInstance();
    }

} // namespace IKore
