#pragma once

#include <glad/glad.h>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Shader.h"

namespace IKore {

// Forward declaration
class Texture;

/**
 * @brief Framebuffer wrapper for render targets
 */
class Framebuffer {
private:
    GLuint m_fbo;
    GLuint m_colorTexture;
    GLuint m_depthTexture;
    int m_width, m_height;
    bool m_hasDepth;

public:
    Framebuffer(int width, int height, bool withDepth = true);
    ~Framebuffer();
    
    // Delete copy operations
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    
    // Move operations
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    
    void bind();
    void unbind();
    void bindColorTexture(GLuint textureUnit = 0);
    void bindDepthTexture(GLuint textureUnit = 0);
    void resize(int width, int height);
    
    // Getters
    GLuint getFramebuffer() const { return m_fbo; }
    GLuint getColorTexture() const { return m_colorTexture; }
    GLuint getDepthTexture() const { return m_depthTexture; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isValid() const;
};

/**
 * @brief Individual post-processing effect interface
 */
class PostProcessEffect {
public:
    virtual ~PostProcessEffect() = default;
    virtual void initialize(int width, int height) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void render(GLuint inputTexture, GLuint outputFramebuffer) = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};

/**
 * @brief Bloom post-processing effect
 */
class BloomEffect : public PostProcessEffect {
private:
    std::shared_ptr<Shader> m_brightFilterShader;
    std::shared_ptr<Shader> m_blurShader;
    std::shared_ptr<Shader> m_combineShader;
    
    std::unique_ptr<Framebuffer> m_brightBuffer;
    std::unique_ptr<Framebuffer> m_blurBuffer1;
    std::unique_ptr<Framebuffer> m_blurBuffer2;
    
    GLuint m_quadVAO, m_quadVBO;
    bool m_enabled;
    float m_threshold;
    float m_intensity;
    int m_blurPasses;

    void setupQuad();
    void renderQuad();

public:
    BloomEffect(float threshold = 1.0f, float intensity = 1.0f, int blurPasses = 5);
    ~BloomEffect();
    
    void initialize(int width, int height) override;
    void resize(int width, int height) override;
    void render(GLuint inputTexture, GLuint outputFramebuffer) override;
    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    // Parameter control
    void setThreshold(float threshold) { m_threshold = threshold; }
    void setIntensity(float intensity) { m_intensity = intensity; }
    void setBlurPasses(int passes) { m_blurPasses = passes; }
    float getThreshold() const { return m_threshold; }
    float getIntensity() const { return m_intensity; }
    int getBlurPasses() const { return m_blurPasses; }
};

/**
 * @brief FXAA (Fast Approximate Anti-Aliasing) effect
 */
class FXAAEffect : public PostProcessEffect {
private:
    std::shared_ptr<Shader> m_fxaaShader;
    GLuint m_quadVAO, m_quadVBO;
    bool m_enabled;
    float m_subPixelQuality;
    float m_edgeThreshold;
    float m_edgeThresholdMin;

    void setupQuad();
    void renderQuad();

public:
    FXAAEffect(float subPixelQuality = 0.75f, float edgeThreshold = 0.125f, float edgeThresholdMin = 0.0625f);
    ~FXAAEffect();
    
    void initialize(int width, int height) override;
    void resize(int width, int height) override;
    void render(GLuint inputTexture, GLuint outputFramebuffer) override;
    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    // Parameter control
    void setSubPixelQuality(float quality) { m_subPixelQuality = quality; }
    void setEdgeThreshold(float threshold) { m_edgeThreshold = threshold; }
    void setEdgeThresholdMin(float threshold) { m_edgeThresholdMin = threshold; }
    float getSubPixelQuality() const { return m_subPixelQuality; }
    float getEdgeThreshold() const { return m_edgeThreshold; }
    float getEdgeThresholdMin() const { return m_edgeThresholdMin; }
};

/**
 * @brief SSAO (Screen-Space Ambient Occlusion) effect
 */
class SSAOEffect : public PostProcessEffect {
private:
    std::shared_ptr<Shader> m_ssaoShader;
    std::shared_ptr<Shader> m_blurShader;
    
    std::unique_ptr<Framebuffer> m_ssaoBuffer;
    std::unique_ptr<Framebuffer> m_blurBuffer;
    
    GLuint m_quadVAO, m_quadVBO;
    GLuint m_noiseTexture;
    std::vector<glm::vec3> m_samples;
    std::vector<glm::vec3> m_noise;
    
    bool m_enabled;
    float m_radius;
    float m_bias;
    float m_intensity;
    int m_sampleCount;

    void setupQuad();
    void renderQuad();
    void generateSamples();
    void generateNoise();

public:
    SSAOEffect(float radius = 0.5f, float bias = 0.025f, float intensity = 1.0f, int sampleCount = 64);
    ~SSAOEffect();
    
    void initialize(int width, int height) override;
    void resize(int width, int height) override;
    void render(GLuint inputTexture, GLuint outputFramebuffer) override;
    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    // Parameter control
    void setRadius(float radius) { m_radius = radius; }
    void setBias(float bias) { m_bias = bias; }
    void setIntensity(float intensity) { m_intensity = intensity; }
    float getRadius() const { return m_radius; }
    float getBias() const { return m_bias; }
    float getIntensity() const { return m_intensity; }
    
    // Requires depth and normal textures
    void renderSSAO(GLuint colorTexture, GLuint depthTexture, GLuint normalTexture, GLuint outputFramebuffer);
};

/**
 * @brief Main post-processing manager
 */
class PostProcessor {
private:
    std::unique_ptr<Framebuffer> m_mainBuffer;
    std::unique_ptr<Framebuffer> m_tempBuffer;
    
    std::unique_ptr<BloomEffect> m_bloomEffect;
    std::unique_ptr<FXAAEffect> m_fxaaEffect;
    std::unique_ptr<SSAOEffect> m_ssaoEffect;
    
    GLuint m_quadVAO, m_quadVBO;
    std::shared_ptr<Shader> m_copyShader;
    
    int m_width, m_height;
    bool m_initialized;

    void setupQuad();
    void renderQuad();

public:
    PostProcessor();
    ~PostProcessor();
    
    // Delete copy operations
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;
    
    void initialize(int width, int height);
    void resize(int width, int height);
    void beginFrame();
    void endFrame();
    
    // Effect management
    BloomEffect* getBloomEffect() { return m_bloomEffect.get(); }
    FXAAEffect* getFXAAEffect() { return m_fxaaEffect.get(); }
    SSAOEffect* getSSAOEffect() { return m_ssaoEffect.get(); }
    
    // Getters
    bool isInitialized() const { return m_initialized; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    Framebuffer* getMainBuffer() { return m_mainBuffer.get(); }
    
    // Manual effect chain control (for advanced usage)
    void renderEffectChain(GLuint inputTexture, GLuint outputFramebuffer = 0);
};

} // namespace IKore
