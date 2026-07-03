#pragma once

#include <glad/glad.h>
#include <memory>
#include "Shader.h"

namespace IKore {

/**
 * @file Ibl.h
 * @brief Image-based-lighting environment generator for PBR (issue #270).
 *
 * At load, convolves a source environment cubemap (the skybox) into the three textures
 * the PBR ambient term needs:
 *   - a small diffuse irradiance cubemap (cosine-weighted convolution),
 *   - a prefiltered specular cubemap with one mip per roughness (GGX importance
 *     sampling), and
 *   - the split-sum BRDF integration LUT (a 2D RG texture).
 *
 * The convolution/integration weights live in the unit-tested render::IblBrdf core;
 * the generation shaders (irradiance_convolution.frag / prefilter.frag /
 * brdf_lut.frag) mirror it. This class owns only the GL objects and the render-to-
 * cubemap passes. It is opt-in: pbr.frag falls back to constant ambient unless these
 * maps are bound and useIBL is set, so a scene with no environment is unchanged.
 */
class IblEnvironment {
public:
    IblEnvironment() = default;
    ~IblEnvironment();

    IblEnvironment(const IblEnvironment&) = delete;
    IblEnvironment& operator=(const IblEnvironment&) = delete;

    /**
     * @brief Generate the irradiance map, prefiltered environment, and BRDF LUT from
     *        @p environmentCubemap. Safe to call again to regenerate. Returns false
     *        (and leaves isValid() false) if shaders or GL resources fail.
     */
    bool generate(GLuint environmentCubemap);

    bool isValid() const { return m_valid; }
    GLuint irradianceMap() const { return m_irradianceMap; }
    GLuint prefilterMap() const { return m_prefilterMap; }
    GLuint brdfLUT() const { return m_brdfLUT; }

    /// Highest prefilter mip index; the shader's maxReflectionLod (LOD = roughness * this).
    float maxReflectionLod() const { return static_cast<float>(kMipLevels - 1); }

    void cleanup();

private:
    static const int kMipLevels = 5; // prefilter roughness levels (LOD 0..4)

    bool loadShaders();
    bool setupGeometry();
    void renderCube();
    void renderQuad();

    bool m_valid{false};
    GLuint m_irradianceMap{0};
    GLuint m_prefilterMap{0};
    GLuint m_brdfLUT{0};
    GLuint m_captureFBO{0};
    GLuint m_captureRBO{0};
    GLuint m_cubeVAO{0};
    GLuint m_cubeVBO{0};
    GLuint m_quadVAO{0};
    GLuint m_quadVBO{0};

    std::shared_ptr<Shader> m_irradianceShader;
    std::shared_ptr<Shader> m_prefilterShader;
    std::shared_ptr<Shader> m_brdfShader;
};

} // namespace IKore
