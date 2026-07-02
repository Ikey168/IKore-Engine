#include "CascadedShadowMap.h"

#include "core/Picking.h"          // pick::Mat4
#include "render/ShadowCascadeBuild.h" // render::buildCascades, render::Cascade
#include "core/Logger.h"

#include <algorithm>

namespace IKore {

namespace {

// glm::mat4 and pick::Mat4 are both column-major, 16 contiguous floats, so a light
// matrix converts by copy in either direction with no transposition.
pick::Mat4 toPick(const glm::mat4& g) {
    pick::Mat4 r{};
    const float* src = &g[0][0];
    for (int i = 0; i < 16; ++i) r.m[i] = src[i];
    return r;
}

glm::mat4 toGlm(const pick::Mat4& p) {
    glm::mat4 g(1.0f);
    float* dst = &g[0][0];
    for (int i = 0; i < 16; ++i) dst[i] = p.m[i];
    return g;
}

} // namespace

CascadedShadowMap::CascadedShadowMap(int cascadeCount, int resolution)
    : m_cascadeCount(std::clamp(cascadeCount, 1, kMaxCascades))
    , m_resolution(resolution > 0 ? resolution : 2048) {}

CascadedShadowMap::~CascadedShadowMap() {
    cleanup();
}

bool CascadedShadowMap::initialize() {
    if (m_initialized) {
        return true;
    }

    glGenFramebuffers(1, &m_framebuffer);

    glGenTextures(1, &m_depthArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 m_resolution, m_resolution, m_cascadeCount,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // White border so samples outside a cascade's fitted box read as fully lit.
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Point the depth attachment at layer 0 just to validate completeness; the real
    // per-cascade layer is selected in beginCascadePass().
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        LOG_ERROR("Cascaded shadow map framebuffer not complete!");
        cleanup();
        return false;
    }

    m_lightMatrices.assign(static_cast<std::size_t>(m_cascadeCount), glm::mat4(1.0f));
    m_splitDepths.assign(static_cast<std::size_t>(m_cascadeCount), 0.0f);

    m_initialized = true;
    LOG_INFO("CascadedShadowMap initialized (" + std::to_string(m_cascadeCount) +
             " cascades, " + std::to_string(m_resolution) + "px)");
    return true;
}

void CascadedShadowMap::computeCascades(const glm::mat4& cameraView, float fovYRadians, float aspect,
                                        float nearZ, float farZ, const glm::vec3& lightDir,
                                        float splitLambda) {
    const ecs::Vec3 dir{lightDir.x, lightDir.y, lightDir.z};
    const std::vector<render::Cascade> cascades =
        render::buildCascades(toPick(cameraView), fovYRadians, aspect, nearZ, farZ, dir,
                              m_cascadeCount, splitLambda);

    m_lightMatrices.assign(static_cast<std::size_t>(m_cascadeCount), glm::mat4(1.0f));
    m_splitDepths.assign(static_cast<std::size_t>(m_cascadeCount), farZ);
    for (std::size_t i = 0; i < cascades.size() && i < static_cast<std::size_t>(m_cascadeCount); ++i) {
        m_lightMatrices[i] = toGlm(cascades[i].lightMatrix);
        m_splitDepths[i] = cascades[i].splitViewDepth;
    }
}

void CascadedShadowMap::beginCascadePass(int index) {
    if (!m_initialized) {
        LOG_WARNING("beginCascadePass on uninitialized CascadedShadowMap");
        return;
    }
    index = std::clamp(index, 0, m_cascadeCount - 1);

    if (index == 0) {
        glGetIntegerv(GL_VIEWPORT, m_previousViewport);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0, index);
    glViewport(0, 0, m_resolution, m_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

void CascadedShadowMap::endShadowPass() {
    if (!m_initialized) {
        return;
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_previousViewport[0], m_previousViewport[1],
               m_previousViewport[2], m_previousViewport[3]);
}

void CascadedShadowMap::bindArray(int textureUnit) const {
    if (!m_initialized) {
        return;
    }
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);
}

void CascadedShadowMap::cleanup() {
    if (m_framebuffer != 0) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }
    if (m_depthArray != 0) {
        glDeleteTextures(1, &m_depthArray);
        m_depthArray = 0;
    }
    m_initialized = false;
}

} // namespace IKore
