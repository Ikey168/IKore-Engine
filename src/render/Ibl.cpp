#include "render/Ibl.h"

#include "core/Logger.h"
#include "render/IblBrdf.h" // shared mip<->roughness mapping (mirrored by the shaders)

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace IKore {

namespace {

// The six capture views for rendering a cubemap (matches GL_TEXTURE_CUBE_MAP_* order:
// +X, -X, +Y, -Y, +Z, -Z), with a 90-degree FOV projection covering one face.
const glm::mat4 kCaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

glm::mat4 captureView(int face) {
    const glm::vec3 eye(0.0f);
    switch (face) {
        case 0: return glm::lookAt(eye, glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0));
        case 1: return glm::lookAt(eye, glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0));
        case 2: return glm::lookAt(eye, glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1));
        case 3: return glm::lookAt(eye, glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1));
        case 4: return glm::lookAt(eye, glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0));
        default: return glm::lookAt(eye, glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
    }
}

} // namespace

IblEnvironment::~IblEnvironment() {
    cleanup();
}

bool IblEnvironment::loadShaders() {
    std::string err;
    m_irradianceShader = Shader::loadFromFilesCached("src/shaders/cubemap.vert",
                                                     "src/shaders/irradiance_convolution.frag", err);
    m_prefilterShader = Shader::loadFromFilesCached("src/shaders/cubemap.vert",
                                                    "src/shaders/prefilter.frag", err);
    m_brdfShader = Shader::loadFromFilesCached("src/shaders/brdf_lut.vert",
                                               "src/shaders/brdf_lut.frag", err);
    if (!m_irradianceShader || !m_prefilterShader || !m_brdfShader) {
        LOG_ERROR("IBL shader load failed: " + err);
        return false;
    }
    return true;
}

bool IblEnvironment::setupGeometry() {
    // Unit cube (positions only) for the cubemap convolution passes.
    const float cube[] = {
        -1, -1, -1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1, -1, -1, -1,  1, -1,
        -1, -1,  1,  1, -1,  1,  1,  1,  1,  1,  1,  1, -1,  1,  1, -1, -1,  1,
        -1,  1,  1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1, -1,  1,  1,
         1,  1,  1,  1, -1, -1,  1,  1, -1,  1, -1, -1,  1,  1,  1,  1, -1,  1,
        -1, -1, -1,  1, -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,  1, -1, -1, -1,
        -1,  1, -1,  1,  1,  1,  1,  1, -1,  1,  1,  1, -1,  1, -1, -1,  1,  1
    };
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindVertexArray(m_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Fullscreen quad (pos.xy, uv) for the BRDF LUT pass; matches brdf_lut.vert.
    const float quad[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

void IblEnvironment::renderCube() {
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void IblEnvironment::renderQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

bool IblEnvironment::generate(GLuint environmentCubemap) {
    if (environmentCubemap == 0) return false;
    cleanup();
    if (!loadShaders() || !setupGeometry()) {
        cleanup();
        return false;
    }

    // Seamless cubemap sampling avoids visible face seams in the convolution.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glGenFramebuffers(1, &m_captureFBO);
    glGenRenderbuffers(1, &m_captureRBO);

    // --- 1. Diffuse irradiance cubemap (small; low-frequency signal). ---
    const int kIrradianceSize = 32;
    glGenTextures(1, &m_irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, kIrradianceSize,
                     kIrradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kIrradianceSize, kIrradianceSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);

    m_irradianceShader->use();
    m_irradianceShader->setMat4("projection", glm::value_ptr(kCaptureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubemap);
    m_irradianceShader->setInt("environmentMap", 0);
    glViewport(0, 0, kIrradianceSize, kIrradianceSize);
    for (int i = 0; i < 6; ++i) {
        m_irradianceShader->setMat4("view", glm::value_ptr(captureView(i)));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }

    // --- 2. Prefiltered specular cubemap (mips = roughness levels). ---
    const int kPrefilterSize = 128;
    glGenTextures(1, &m_prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, kPrefilterSize,
                     kPrefilterSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    m_prefilterShader->use();
    m_prefilterShader->setMat4("projection", glm::value_ptr(kCaptureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubemap);
    m_prefilterShader->setInt("environmentMap", 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    for (int mip = 0; mip < kMipLevels; ++mip) {
        const int mipSize = static_cast<int>(kPrefilterSize * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);

        const float roughness = render::mipToRoughness(mip, kMipLevels - 1);
        m_prefilterShader->setFloat("roughness", roughness);
        for (int i = 0; i < 6; ++i) {
            m_prefilterShader->setMat4("view", glm::value_ptr(captureView(i)));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
    }

    // --- 3. Split-sum BRDF integration LUT (2D RG). ---
    const int kLutSize = 512;
    glGenTextures(1, &m_brdfLUT);
    glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, kLutSize, kLutSize, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kLutSize, kLutSize);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
    glViewport(0, 0, kLutSize, kLutSize);
    m_brdfShader->use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();

    // Restore default framebuffer + viewport.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    m_valid = m_irradianceMap != 0 && m_prefilterMap != 0 && m_brdfLUT != 0;
    if (m_valid) {
        LOG_INFO("IBL environment generated (irradiance + prefilter + BRDF LUT)");
    }
    return m_valid;
}

void IblEnvironment::cleanup() {
    if (m_irradianceMap) { glDeleteTextures(1, &m_irradianceMap); m_irradianceMap = 0; }
    if (m_prefilterMap) { glDeleteTextures(1, &m_prefilterMap); m_prefilterMap = 0; }
    if (m_brdfLUT) { glDeleteTextures(1, &m_brdfLUT); m_brdfLUT = 0; }
    if (m_captureFBO) { glDeleteFramebuffers(1, &m_captureFBO); m_captureFBO = 0; }
    if (m_captureRBO) { glDeleteRenderbuffers(1, &m_captureRBO); m_captureRBO = 0; }
    if (m_cubeVAO) { glDeleteVertexArrays(1, &m_cubeVAO); m_cubeVAO = 0; }
    if (m_cubeVBO) { glDeleteBuffers(1, &m_cubeVBO); m_cubeVBO = 0; }
    if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
    if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO); m_quadVBO = 0; }
    m_valid = false;
}

} // namespace IKore
