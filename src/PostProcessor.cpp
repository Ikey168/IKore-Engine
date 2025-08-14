#include "PostProcessor.h"
#include "core/Logger.h"
#include <random>
#include <algorithm>

namespace IKore {

// ===== Framebuffer Implementation =====

Framebuffer::Framebuffer(int width, int height, bool withDepth) 
    : m_fbo(0), m_colorTexture(0), m_depthTexture(0), m_width(width), m_height(height), m_hasDepth(withDepth) {
    
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    // Create color texture
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);
    
    // Create depth texture if requested
    if (withDepth) {
        glGenTextures(1, &m_depthTexture);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    }
    
    if (!isValid()) {
        LOG_ERROR("Failed to create framebuffer");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Framebuffer::~Framebuffer() {
    if (m_colorTexture) glDeleteTextures(1, &m_colorTexture);
    if (m_depthTexture) glDeleteTextures(1, &m_depthTexture);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_fbo(other.m_fbo), m_colorTexture(other.m_colorTexture), m_depthTexture(other.m_depthTexture),
      m_width(other.m_width), m_height(other.m_height), m_hasDepth(other.m_hasDepth) {
    other.m_fbo = other.m_colorTexture = other.m_depthTexture = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        if (m_colorTexture) glDeleteTextures(1, &m_colorTexture);
        if (m_depthTexture) glDeleteTextures(1, &m_depthTexture);
        if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
        
        // Move data
        m_fbo = other.m_fbo;
        m_colorTexture = other.m_colorTexture;
        m_depthTexture = other.m_depthTexture;
        m_width = other.m_width;
        m_height = other.m_height;
        m_hasDepth = other.m_hasDepth;
        
        // Reset other
        other.m_fbo = other.m_colorTexture = other.m_depthTexture = 0;
    }
    return *this;
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bindColorTexture(GLuint textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
}

void Framebuffer::bindDepthTexture(GLuint textureUnit) {
    if (m_hasDepth) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    }
}

void Framebuffer::resize(int width, int height) {
    if (width == m_width && height == m_height) return;
    
    m_width = width;
    m_height = height;
    
    // Resize color texture
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    
    // Resize depth texture if present
    if (m_hasDepth) {
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
}

bool Framebuffer::isValid() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

// ===== BloomEffect Implementation =====

BloomEffect::BloomEffect(float threshold, float intensity, int blurPasses)
    : m_quadVAO(0), m_quadVBO(0), m_enabled(true), m_threshold(threshold), 
      m_intensity(intensity), m_blurPasses(blurPasses) {
}

BloomEffect::~BloomEffect() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void BloomEffect::initialize(int width, int height) {
    setupQuad();
    
    // Create framebuffers
    m_brightBuffer = std::make_unique<Framebuffer>(width, height, false);
    m_blurBuffer1 = std::make_unique<Framebuffer>(width / 2, height / 2, false);
    m_blurBuffer2 = std::make_unique<Framebuffer>(width / 2, height / 2, false);
    
    // Load shaders
    std::string shaderError;
    m_brightFilterShader = Shader::loadFromFilesCached("src/shaders/bloom_bright.vert", "src/shaders/bloom_bright.frag", shaderError);
    m_blurShader = Shader::loadFromFilesCached("src/shaders/bloom_blur.vert", "src/shaders/bloom_blur.frag", shaderError);
    m_combineShader = Shader::loadFromFilesCached("src/shaders/bloom_combine.vert", "src/shaders/bloom_combine.frag", shaderError);
    
    if (!m_brightFilterShader || !m_blurShader || !m_combineShader) {
        LOG_ERROR("Failed to load bloom shaders: " + shaderError);
    }
}

void BloomEffect::resize(int width, int height) {
    if (m_brightBuffer) m_brightBuffer->resize(width, height);
    if (m_blurBuffer1) m_blurBuffer1->resize(width / 2, height / 2);
    if (m_blurBuffer2) m_blurBuffer2->resize(width / 2, height / 2);
}

void BloomEffect::render(GLuint inputTexture, GLuint outputFramebuffer) {
    if (!m_enabled || !m_brightFilterShader || !m_blurShader || !m_combineShader) return;
    
    glDisable(GL_DEPTH_TEST);
    
    // Step 1: Extract bright pixels
    m_brightBuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_brightFilterShader->use();
    m_brightFilterShader->setFloat("threshold", m_threshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(glGetUniformLocation(m_brightFilterShader->id(), "scene"), 0);
    renderQuad();
    
    // Step 2: Blur bright pixels
    bool horizontal = true;
    bool first_iteration = true;
    for (int i = 0; i < m_blurPasses * 2; i++) {
        if (horizontal) {
            m_blurBuffer1->bind();
        } else {
            m_blurBuffer2->bind();
        }
        
        m_blurShader->use();
        m_blurShader->setInt("horizontal", horizontal ? 1 : 0);
        
        glActiveTexture(GL_TEXTURE0);
        if (first_iteration) {
            m_brightBuffer->bindColorTexture(0);
        } else {
            if (horizontal) {
                m_blurBuffer2->bindColorTexture(0);
            } else {
                m_blurBuffer1->bindColorTexture(0);
            }
        }
        glUniform1i(glGetUniformLocation(m_blurShader->id(), "image"), 0);
        renderQuad();
        
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }
    
    // Step 3: Combine with original scene
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    m_combineShader->use();
    m_combineShader->setFloat("bloomIntensity", m_intensity);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(glGetUniformLocation(m_combineShader->id(), "scene"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    m_blurBuffer2->bindColorTexture(1);
    glUniform1i(glGetUniformLocation(m_combineShader->id(), "bloomBlur"), 1);
    
    renderQuad();
    
    glEnable(GL_DEPTH_TEST);
}

void BloomEffect::setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void BloomEffect::renderQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ===== FXAAEffect Implementation =====

FXAAEffect::FXAAEffect(float subPixelQuality, float edgeThreshold, float edgeThresholdMin)
    : m_quadVAO(0), m_quadVBO(0), m_enabled(true), m_subPixelQuality(subPixelQuality),
      m_edgeThreshold(edgeThreshold), m_edgeThresholdMin(edgeThresholdMin) {
}

FXAAEffect::~FXAAEffect() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void FXAAEffect::initialize(int /*width*/, int /*height*/) {
    setupQuad();
    
    std::string shaderError;
    m_fxaaShader = Shader::loadFromFilesCached("src/shaders/fxaa.vert", "src/shaders/fxaa.frag", shaderError);
    
    if (!m_fxaaShader) {
        LOG_ERROR("Failed to load FXAA shader: " + shaderError);
    }
}

void FXAAEffect::resize(int /*width*/, int /*height*/) {
    // FXAA doesn't need additional framebuffers
}

void FXAAEffect::render(GLuint inputTexture, GLuint outputFramebuffer) {
    if (!m_enabled || !m_fxaaShader) return;
    
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    glDisable(GL_DEPTH_TEST);
    
    m_fxaaShader->use();
    m_fxaaShader->setFloat("u_subPixelQuality", m_subPixelQuality);
    m_fxaaShader->setFloat("u_edgeThreshold", m_edgeThreshold);
    m_fxaaShader->setFloat("u_edgeThresholdMin", m_edgeThresholdMin);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(glGetUniformLocation(m_fxaaShader->id(), "u_colorTexture"), 0);
    
    renderQuad();
    
    glEnable(GL_DEPTH_TEST);
}

void FXAAEffect::setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void FXAAEffect::renderQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ===== SSAOEffect Implementation =====

SSAOEffect::SSAOEffect(float radius, float bias, float intensity, int sampleCount)
    : m_quadVAO(0), m_quadVBO(0), m_noiseTexture(0), m_enabled(true),
      m_radius(radius), m_bias(bias), m_intensity(intensity), m_sampleCount(sampleCount) {
}

SSAOEffect::~SSAOEffect() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_noiseTexture) glDeleteTextures(1, &m_noiseTexture);
}

void SSAOEffect::initialize(int width, int height) {
    setupQuad();
    generateSamples();
    generateNoise();
    
    // Create framebuffers
    m_ssaoBuffer = std::make_unique<Framebuffer>(width, height, false);
    m_blurBuffer = std::make_unique<Framebuffer>(width, height, false);
    
    std::string shaderError;
    m_ssaoShader = Shader::loadFromFilesCached("src/shaders/ssao.vert", "src/shaders/ssao.frag", shaderError);
    m_blurShader = Shader::loadFromFilesCached("src/shaders/ssao_blur.vert", "src/shaders/ssao_blur.frag", shaderError);
    
    if (!m_ssaoShader || !m_blurShader) {
        LOG_ERROR("Failed to load SSAO shaders: " + shaderError);
    }
}

void SSAOEffect::resize(int width, int height) {
    if (m_ssaoBuffer) m_ssaoBuffer->resize(width, height);
    if (m_blurBuffer) m_blurBuffer->resize(width, height);
}

void SSAOEffect::render(GLuint inputTexture, GLuint outputFramebuffer) {
    // SSAO requires depth and normal textures, so this is a simplified version
    // In a real implementation, you'd pass these as parameters
    if (!m_enabled || !m_ssaoShader || !m_blurBuffer) return;
    
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    glDisable(GL_DEPTH_TEST);
    
    // For now, just pass through the input texture
    // TODO: Implement full SSAO when G-buffer is available
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    renderQuad();
    
    glEnable(GL_DEPTH_TEST);
}

void SSAOEffect::renderSSAO(GLuint /*colorTexture*/, GLuint depthTexture, GLuint normalTexture, GLuint outputFramebuffer) {
    if (!m_enabled || !m_ssaoShader || !m_blurShader) return;
    
    glDisable(GL_DEPTH_TEST);
    
    // Generate SSAO texture
    m_ssaoBuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_ssaoShader->use();
    
    // Set uniforms
    m_ssaoShader->setFloat("radius", m_radius);
    m_ssaoShader->setFloat("bias", m_bias);
    m_ssaoShader->setInt("sampleCount", m_sampleCount);
    
    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(glGetUniformLocation(m_ssaoShader->id(), "gDepth"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glUniform1i(glGetUniformLocation(m_ssaoShader->id(), "gNormal"), 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
    glUniform1i(glGetUniformLocation(m_ssaoShader->id(), "texNoise"), 2);
    
    renderQuad();
    
    // Blur SSAO texture to remove noise
    m_blurBuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurShader->use();
    m_ssaoBuffer->bindColorTexture(0);
    glUniform1i(glGetUniformLocation(m_blurShader->id(), "ssaoInput"), 0);
    renderQuad();
    
    // Apply SSAO to final image
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    // TODO: Combine SSAO with lighting calculation
    
    glEnable(GL_DEPTH_TEST);
}

void SSAOEffect::setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void SSAOEffect::renderQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SSAOEffect::generateSamples() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    m_samples.clear();
    for (int i = 0; i < m_sampleCount; ++i) {
        glm::vec3 sample(
            dis(gen) * 2.0 - 1.0,
            dis(gen) * 2.0 - 1.0,
            dis(gen)
        );
        sample = glm::normalize(sample);
        sample *= dis(gen);
        
        // Scale samples s.t. they're more aligned to center of kernel
        float scale = (float)i / (float)m_sampleCount;
        scale = 0.1f + (scale * scale) * 0.9f; // lerp(0.1f, 1.0f, scale * scale)
        sample *= scale;
        m_samples.push_back(sample);
    }
}

void SSAOEffect::generateNoise() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    m_noise.clear();
    for (int i = 0; i < 16; i++) {
        glm::vec3 noise(
            dis(gen) * 2.0 - 1.0,
            dis(gen) * 2.0 - 1.0,
            0.0f
        );
        m_noise.push_back(noise);
    }
    
    glGenTextures(1, &m_noiseTexture);
    glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &m_noise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

// ===== PostProcessor Implementation =====

PostProcessor::PostProcessor() 
    : m_quadVAO(0), m_quadVBO(0), m_width(0), m_height(0), m_initialized(false) {
}

PostProcessor::~PostProcessor() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void PostProcessor::initialize(int width, int height) {
    m_width = width;
    m_height = height;
    
    setupQuad();
    
    // Create main framebuffers
    m_mainBuffer = std::make_unique<Framebuffer>(width, height, true);
    m_tempBuffer = std::make_unique<Framebuffer>(width, height, false);
    
    // Initialize effects
    m_bloomEffect = std::make_unique<BloomEffect>();
    m_fxaaEffect = std::make_unique<FXAAEffect>();
    m_ssaoEffect = std::make_unique<SSAOEffect>();
    
    m_bloomEffect->initialize(width, height);
    m_fxaaEffect->initialize(width, height);
    m_ssaoEffect->initialize(width, height);
    
    // Load copy shader
    std::string shaderError;
    m_copyShader = Shader::loadFromFilesCached("src/shaders/copy.vert", "src/shaders/copy.frag", shaderError);
    
    if (!m_copyShader) {
        LOG_ERROR("Failed to load copy shader: " + shaderError);
    }
    
    m_initialized = true;
    LOG_INFO("PostProcessor initialized with resolution: " + std::to_string(width) + "x" + std::to_string(height));
}

void PostProcessor::resize(int width, int height) {
    if (!m_initialized || (width == m_width && height == m_height)) return;
    
    m_width = width;
    m_height = height;
    
    if (m_mainBuffer) m_mainBuffer->resize(width, height);
    if (m_tempBuffer) m_tempBuffer->resize(width, height);
    
    if (m_bloomEffect) m_bloomEffect->resize(width, height);
    if (m_fxaaEffect) m_fxaaEffect->resize(width, height);
    if (m_ssaoEffect) m_ssaoEffect->resize(width, height);
}

void PostProcessor::beginFrame() {
    if (!m_initialized || !m_mainBuffer) return;
    
    m_mainBuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessor::endFrame() {
    if (!m_initialized || !m_mainBuffer) return;
    
    m_mainBuffer->unbind();
    renderEffectChain(m_mainBuffer->getColorTexture(), 0);
}

void PostProcessor::renderEffectChain(GLuint inputTexture, GLuint outputFramebuffer) {
    if (!m_initialized) return;
    
    GLuint currentTexture = inputTexture;
    
    // Apply effects in order: SSAO -> Bloom -> FXAA
    
    // SSAO (if enabled)
    if (m_ssaoEffect && m_ssaoEffect->isEnabled()) {
        // Note: SSAO needs special handling with G-buffer
        // For now, we skip it in the basic pipeline
    }
    
    // Bloom (if enabled)
    if (m_bloomEffect && m_bloomEffect->isEnabled()) {
        if (m_tempBuffer) {
            m_bloomEffect->render(currentTexture, m_tempBuffer->getFramebuffer());
            currentTexture = m_tempBuffer->getColorTexture();
        }
    }
    
    // FXAA (if enabled) - always render to output
    if (m_fxaaEffect && m_fxaaEffect->isEnabled()) {
        m_fxaaEffect->render(currentTexture, outputFramebuffer);
    } else {
        // If no FXAA, copy current texture to output
        glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
        if (m_copyShader) {
            glDisable(GL_DEPTH_TEST);
            m_copyShader->use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentTexture);
            glUniform1i(glGetUniformLocation(m_copyShader->id(), "screenTexture"), 0);
            renderQuad();
            glEnable(GL_DEPTH_TEST);
        }
    }
}

void PostProcessor::setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void PostProcessor::renderQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

} // namespace IKore
