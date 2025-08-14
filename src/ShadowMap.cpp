#include "ShadowMap.h"
#include "Shader.h"
#include "core/Logger.h"
#include <algorithm>
#include <iostream>

namespace IKore {

ShadowMap::ShadowMap(Type type, int resolution)
    : m_type(type)
    , m_resolution(resolution)
    , m_initialized(false)
    , m_framebuffer(0)
    , m_depthTexture(0)
    , m_lightSpaceMatrix(1.0f)
    , m_softShadows(true)
    , m_pcfKernelSize(3)
    , m_shadowBias(0.005f)
{
    m_pointLightMatrices.resize(6);
    for (int i = 0; i < 4; i++) {
        m_previousViewport[i] = 0;
    }
}

ShadowMap::~ShadowMap() {
    cleanup();
}

bool ShadowMap::initialize() {
    if (m_initialized) {
        return true;
    }

    bool success = false;
    if (m_type == Type::DIRECTIONAL) {
        success = setupDirectionalShadowMap();
    } else {
        success = setupPointShadowMap();
    }

    if (success) {
        m_initialized = true;
        LOG_INFO("ShadowMap initialized successfully (Type: " + 
                std::string(m_type == Type::DIRECTIONAL ? "Directional" : "Point") + 
                ", Resolution: " + std::to_string(m_resolution) + ")");
    } else {
        LOG_ERROR("Failed to initialize ShadowMap");
        cleanup();
    }

    return success;
}

bool ShadowMap::setupDirectionalShadowMap() {
    // Generate framebuffer
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // Generate depth texture
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    
    // Create depth texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_resolution, m_resolution, 
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    // Set texture parameters for shadow mapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    // Set border color to white (1.0) so areas outside shadow map are lit
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    
    // Don't render to color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("Directional shadow map framebuffer not complete!");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool ShadowMap::setupPointShadowMap() {
    // Generate framebuffer
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // Generate depth cubemap texture
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthTexture);
    
    // Create depth cubemap faces
    for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24, 
                     m_resolution, m_resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Attach depth cubemap to framebuffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthTexture, 0);
    
    // Don't render to color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("Point light shadow map framebuffer not complete!");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void ShadowMap::beginShadowPass() {
    if (!m_initialized) {
        LOG_WARNING("Attempting to begin shadow pass on uninitialized shadow map");
        return;
    }

    // Store current viewport
    glGetIntegerv(GL_VIEWPORT, m_previousViewport);

    // Bind shadow map framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_resolution, m_resolution);
    
    // Clear depth buffer
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Enable depth testing and disable color writes
    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    
    // Optional: Enable polygon offset to reduce shadow acne
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

void ShadowMap::endShadowPass() {
    if (!m_initialized) {
        return;
    }

    // Disable polygon offset
    glDisable(GL_POLYGON_OFFSET_FILL);
    
    // Re-enable color writes
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    
    // Restore previous framebuffer and viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_previousViewport[0], m_previousViewport[1], 
               m_previousViewport[2], m_previousViewport[3]);
}

void ShadowMap::setLightSpaceMatrix(const glm::vec3& lightDirection, 
                                  const glm::vec3& sceneCenter, 
                                  float sceneRadius) {
    if (m_type != Type::DIRECTIONAL) {
        LOG_WARNING("setLightSpaceMatrix called on non-directional shadow map");
        return;
    }

    // Create orthographic projection for directional light
    float left = -sceneRadius;
    float right = sceneRadius;
    float bottom = -sceneRadius;
    float top = sceneRadius;
    float nearPlane = 1.0f;
    float farPlane = sceneRadius * 2.0f + 10.0f;
    
    glm::mat4 lightProjection = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    
    // Create view matrix looking along light direction
    glm::vec3 lightPos = sceneCenter - lightDirection * sceneRadius;
    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    
    m_lightSpaceMatrix = lightProjection * lightView;
}

void ShadowMap::setPointLightMatrices(const glm::vec3& lightPosition, float shadowDistance) {
    if (m_type != Type::POINT) {
        LOG_WARNING("setPointLightMatrices called on non-point shadow map");
        return;
    }

    // Create perspective projection for point light
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, shadowDistance);
    
    // Create 6 view matrices for cubemap faces
    m_pointLightMatrices[0] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)); // +X
    m_pointLightMatrices[1] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)); // -X
    m_pointLightMatrices[2] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)); // +Y
    m_pointLightMatrices[3] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)); // -Y
    m_pointLightMatrices[4] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)); // +Z
    m_pointLightMatrices[5] = shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)); // -Z
}

void ShadowMap::bindShadowMap(int textureUnit) {
    if (!m_initialized) {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    if (m_type == Type::DIRECTIONAL) {
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    } else {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthTexture);
    }
}

void ShadowMap::unbindShadowMap() {
    if (!m_initialized) {
        return;
    }

    if (m_type == Type::DIRECTIONAL) {
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
}

void ShadowMap::setPCFKernelSize(int size) {
    // Only allow odd sizes (1, 3, 5, 7)
    if (size % 2 == 0) {
        size = std::max(1, size - 1);
    }
    m_pcfKernelSize = std::clamp(size, 1, 7);
}

void ShadowMap::cleanup() {
    if (m_framebuffer != 0) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    m_initialized = false;
}

// ShadowMapManager implementation
ShadowMapManager::ShadowMapManager()
    : m_enabled(true) {
}

ShadowMapManager::~ShadowMapManager() {
    m_shadowMaps.clear();
    m_lightIndices.clear();
}

bool ShadowMapManager::initialize() {
    LOG_INFO("ShadowMapManager initialized");
    return true;
}

ShadowMap* ShadowMapManager::createDirectionalShadowMap(int lightIndex, int resolution) {
    // Check if shadow map already exists for this light
    auto existingMap = getShadowMap(lightIndex);
    if (existingMap != nullptr) {
        LOG_WARNING("Shadow map already exists for light index " + std::to_string(lightIndex));
        return existingMap;
    }

    auto shadowMap = std::make_unique<ShadowMap>(ShadowMap::Type::DIRECTIONAL, resolution);
    if (!shadowMap->initialize()) {
        LOG_ERROR("Failed to create directional shadow map for light " + std::to_string(lightIndex));
        return nullptr;
    }

    ShadowMap* result = shadowMap.get();
    m_shadowMaps.push_back(std::move(shadowMap));
    m_lightIndices.push_back(lightIndex);

    LOG_INFO("Created directional shadow map for light " + std::to_string(lightIndex));
    return result;
}

ShadowMap* ShadowMapManager::createPointShadowMap(int lightIndex, int resolution) {
    // Check if shadow map already exists for this light
    auto existingMap = getShadowMap(lightIndex);
    if (existingMap != nullptr) {
        LOG_WARNING("Shadow map already exists for light index " + std::to_string(lightIndex));
        return existingMap;
    }

    auto shadowMap = std::make_unique<ShadowMap>(ShadowMap::Type::POINT, resolution);
    if (!shadowMap->initialize()) {
        LOG_ERROR("Failed to create point shadow map for light " + std::to_string(lightIndex));
        return nullptr;
    }

    ShadowMap* result = shadowMap.get();
    m_shadowMaps.push_back(std::move(shadowMap));
    m_lightIndices.push_back(lightIndex);

    LOG_INFO("Created point shadow map for light " + std::to_string(lightIndex));
    return result;
}

ShadowMap* ShadowMapManager::getShadowMap(int lightIndex) {
    auto it = std::find(m_lightIndices.begin(), m_lightIndices.end(), lightIndex);
    if (it != m_lightIndices.end()) {
        size_t index = std::distance(m_lightIndices.begin(), it);
        return m_shadowMaps[index].get();
    }
    return nullptr;
}

void ShadowMapManager::removeShadowMap(int lightIndex) {
    auto it = std::find(m_lightIndices.begin(), m_lightIndices.end(), lightIndex);
    if (it != m_lightIndices.end()) {
        size_t index = std::distance(m_lightIndices.begin(), it);
        m_shadowMaps.erase(m_shadowMaps.begin() + index);
        m_lightIndices.erase(it);
        LOG_INFO("Removed shadow map for light " + std::to_string(lightIndex));
    }
}

void ShadowMapManager::updateShadowMaps() {
    // This method can be used for any per-frame updates if needed
    // Currently just a placeholder for future enhancements
}

void ShadowMapManager::setShadowQuality(float quality) {
    quality = std::clamp(quality, 0.0f, 1.0f);
    
    // Adjust shadow map parameters based on quality
    for (auto& shadowMap : m_shadowMaps) {
        if (quality < 0.3f) {
            shadowMap->setSoftShadows(false);
            shadowMap->setPCFKernelSize(1);
        } else if (quality < 0.6f) {
            shadowMap->setSoftShadows(true);
            shadowMap->setPCFKernelSize(3);
        } else {
            shadowMap->setSoftShadows(true);
            shadowMap->setPCFKernelSize(5);
        }
    }
    
    LOG_INFO("Shadow quality set to " + std::to_string(quality));
}

} // namespace IKore
