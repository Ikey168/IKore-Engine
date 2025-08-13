#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace IKore {

class Shader;

/**
 * ShadowMap class for managing shadow mapping functionality
 * Supports both directional light shadows and point light shadow cubemaps
 */
class ShadowMap {
public:
    enum class Type {
        DIRECTIONAL,    // Traditional 2D shadow map for directional lights
        POINT          // Cubemap shadow map for point lights (omnidirectional)
    };

    /**
     * Constructor
     * @param type The type of shadow map (directional or point light)
     * @param resolution Shadow map resolution (width and height)
     */
    ShadowMap(Type type, int resolution = 2048);
    
    /**
     * Destructor - cleanup OpenGL resources
     */
    ~ShadowMap();

    /**
     * Initialize the shadow map framebuffer and textures
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * Begin shadow map rendering pass
     * Sets up the framebuffer and viewport for shadow rendering
     */
    void beginShadowPass();

    /**
     * End shadow map rendering pass
     * Restores previous framebuffer and viewport
     */
    void endShadowPass();

    /**
     * Calculate and set the light space transformation matrix for directional lights
     * @param lightDirection The direction of the light
     * @param sceneCenter Center of the scene to shadow
     * @param sceneRadius Radius of the scene to shadow
     */
    void setLightSpaceMatrix(const glm::vec3& lightDirection, 
                           const glm::vec3& sceneCenter = glm::vec3(0.0f),
                           float sceneRadius = 20.0f);

    /**
     * Calculate and set the light space matrices for point light shadows (6 faces)
     * @param lightPosition Position of the point light
     * @param shadowDistance Maximum shadow casting distance
     */
    void setPointLightMatrices(const glm::vec3& lightPosition, float shadowDistance = 25.0f);

    /**
     * Bind the shadow map texture(s) for sampling in lighting shaders
     * @param textureUnit The texture unit to bind to (GL_TEXTURE0 + unit)
     */
    void bindShadowMap(int textureUnit);

    /**
     * Unbind the shadow map texture(s)
     */
    void unbindShadowMap();

    /**
     * Get the light space transformation matrix (for directional lights)
     */
    const glm::mat4& getLightSpaceMatrix() const { return m_lightSpaceMatrix; }

    /**
     * Get the light space matrices for point lights (6 faces)
     */
    const std::vector<glm::mat4>& getPointLightMatrices() const { return m_pointLightMatrices; }

    /**
     * Get the shadow map texture ID
     */
    GLuint getShadowMapTexture() const { return m_depthTexture; }

    /**
     * Get the shadow map resolution
     */
    int getResolution() const { return m_resolution; }

    /**
     * Get the shadow map type
     */
    Type getType() const { return m_type; }

    /**
     * Enable/disable soft shadows (PCF)
     */
    void setSoftShadows(bool enabled) { m_softShadows = enabled; }
    bool getSoftShadows() const { return m_softShadows; }

    /**
     * Set PCF kernel size for soft shadows (1, 3, 5, or 7)
     */
    void setPCFKernelSize(int size);
    int getPCFKernelSize() const { return m_pcfKernelSize; }

    /**
     * Set shadow bias to reduce shadow acne
     */
    void setShadowBias(float bias) { m_shadowBias = bias; }
    float getShadowBias() const { return m_shadowBias; }

    /**
     * Check if shadow map is properly initialized
     */
    bool isInitialized() const { return m_initialized; }

private:
    Type m_type;
    int m_resolution;
    bool m_initialized;

    // OpenGL objects
    GLuint m_framebuffer;
    GLuint m_depthTexture;

    // Transformation matrices
    glm::mat4 m_lightSpaceMatrix;
    std::vector<glm::mat4> m_pointLightMatrices;  // 6 matrices for cubemap faces

    // Shadow parameters
    bool m_softShadows;
    int m_pcfKernelSize;
    float m_shadowBias;

    // Store previous viewport for restoration
    GLint m_previousViewport[4];

    /**
     * Setup framebuffer for directional light shadows
     */
    bool setupDirectionalShadowMap();

    /**
     * Setup framebuffer for point light shadows (cubemap)
     */
    bool setupPointShadowMap();

    /**
     * Cleanup OpenGL resources
     */
    void cleanup();
};

/**
 * ShadowMapManager class for managing multiple shadow maps
 * Handles shadow maps for multiple lights efficiently
 */
class ShadowMapManager {
public:
    ShadowMapManager();
    ~ShadowMapManager();

    /**
     * Initialize the shadow map manager
     */
    bool initialize();

    /**
     * Create a shadow map for a directional light
     * @param lightIndex Index of the light (for identification)
     * @param resolution Shadow map resolution
     * @return Pointer to the created shadow map, or nullptr on failure
     */
    ShadowMap* createDirectionalShadowMap(int lightIndex, int resolution = 2048);

    /**
     * Create a shadow map for a point light
     * @param lightIndex Index of the light (for identification)
     * @param resolution Shadow map resolution
     * @return Pointer to the created shadow map, or nullptr on failure
     */
    ShadowMap* createPointShadowMap(int lightIndex, int resolution = 1024);

    /**
     * Get shadow map for a specific light
     * @param lightIndex Index of the light
     * @return Pointer to shadow map, or nullptr if not found
     */
    ShadowMap* getShadowMap(int lightIndex);

    /**
     * Remove shadow map for a specific light
     * @param lightIndex Index of the light
     */
    void removeShadowMap(int lightIndex);

    /**
     * Update all shadow maps (call before rendering shadows)
     */
    void updateShadowMaps();

    /**
     * Enable/disable shadows globally
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /**
     * Set global shadow quality (affects resolution and PCF)
     */
    void setShadowQuality(float quality); // 0.0 to 1.0

private:
    bool m_enabled;
    std::vector<std::unique_ptr<ShadowMap>> m_shadowMaps;
    std::vector<int> m_lightIndices;
};

} // namespace IKore
