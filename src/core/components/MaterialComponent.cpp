#include "MaterialComponent.h"
#include "../../Texture.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>

namespace IKore {

    // Static texture cache initialization
    std::unordered_map<std::string, std::shared_ptr<Texture>> MaterialComponent::textureCache_;

    void MaterialComponent::setMaterial(const Material& material) {
        material_ = material;
    }

    // === Texture Management ===

    bool MaterialComponent::setDiffuseTexture(const std::string& path) {
        auto texture = loadTextureFromPath(path);
        if (texture) {
            material_.diffuse = texture;
            return true;
        }
        return false;
    }

    void MaterialComponent::setDiffuseTexture(std::shared_ptr<Texture> texture) {
        material_.diffuse = texture;
    }

    bool MaterialComponent::setSpecularTexture(const std::string& path) {
        auto texture = loadTextureFromPath(path);
        if (texture) {
            material_.specular = texture;
            return true;
        }
        return false;
    }

    void MaterialComponent::setSpecularTexture(std::shared_ptr<Texture> texture) {
        material_.specular = texture;
    }

    bool MaterialComponent::setNormalTexture(const std::string& path) {
        auto texture = loadTextureFromPath(path);
        if (texture) {
            material_.normal = texture;
            return true;
        }
        return false;
    }

    void MaterialComponent::setNormalTexture(std::shared_ptr<Texture> texture) {
        material_.normal = texture;
    }

    bool MaterialComponent::setHeightTexture(const std::string& path) {
        auto texture = loadTextureFromPath(path);
        if (texture) {
            material_.height = texture;
            return true;
        }
        return false;
    }

    void MaterialComponent::setHeightTexture(std::shared_ptr<Texture> texture) {
        material_.height = texture;
    }

    bool MaterialComponent::setEmissiveTexture(const std::string& path) {
        auto texture = loadTextureFromPath(path);
        if (texture) {
            material_.emissive = texture;
            return true;
        }
        return false;
    }

    void MaterialComponent::setEmissiveTexture(std::shared_ptr<Texture> texture) {
        material_.emissive = texture;
    }

    void MaterialComponent::clearTexture(const std::string& textureType) {
        if (textureType == "diffuse") {
            material_.diffuse.reset();
        } else if (textureType == "specular") {
            material_.specular.reset();
        } else if (textureType == "normal") {
            material_.normal.reset();
        } else if (textureType == "height") {
            material_.height.reset();
        } else if (textureType == "emissive") {
            material_.emissive.reset();
        }
    }

    void MaterialComponent::clearAllTextures() {
        material_.diffuse.reset();
        material_.specular.reset();
        material_.normal.reset();
        material_.height.reset();
        material_.emissive.reset();
    }

    // === Shader Management ===

    bool MaterialComponent::loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
        try {
            shader_ = Shader::loadFromFilesCached(vertexPath, fragmentPath);
            return shader_ != nullptr;
        } catch (const std::exception& e) {
            std::cerr << "MaterialComponent: Failed to load shader from " 
                      << vertexPath << " and " << fragmentPath << ": " << e.what() << std::endl;
            return false;
        }
    }

    // === Custom Shader Parameters ===

    void MaterialComponent::setShaderParameter(const std::string& name, float value) {
        floatParams_[name] = value;
    }

    void MaterialComponent::setShaderParameter(const std::string& name, const glm::vec3& value) {
        vec3Params_[name] = value;
    }

    void MaterialComponent::setShaderParameter(const std::string& name, const glm::vec4& value) {
        vec4Params_[name] = value;
    }

    void MaterialComponent::setShaderParameter(const std::string& name, int value) {
        intParams_[name] = value;
    }

    void MaterialComponent::removeShaderParameter(const std::string& name) {
        floatParams_.erase(name);
        vec3Params_.erase(name);
        vec4Params_.erase(name);
        intParams_.erase(name);
    }

    void MaterialComponent::clearShaderParameters() {
        floatParams_.clear();
        vec3Params_.clear();
        vec4Params_.clear();
        intParams_.clear();
    }

    // === Rendering Integration ===

    void MaterialComponent::apply() const {
        if (!shader_) {
            std::cerr << "MaterialComponent: No shader assigned for material application" << std::endl;
            return;
        }

        // Use the shader
        shader_->use();

        // Bind textures
        bindTextures();

        // Set material uniforms
        setShaderUniforms();

        // Apply custom parameters
        applyCustomParameters();
    }

    void MaterialComponent::apply(const glm::mat4& modelMatrix) const {
        apply();
        
        if (shader_) {
            // Set model matrix if shader supports it
            try {
                shader_->setMat4("model", modelMatrix);
            } catch (const std::exception& e) {
                // Shader might not have model uniform, which is fine
            }
        }
    }

    void MaterialComponent::unbind() const {
        // Unbind textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Reset to texture unit 0
        glActiveTexture(GL_TEXTURE0);
    }

    // === Utility Functions ===

    bool MaterialComponent::hasTextures() const {
        return material_.hasDiffuseTexture() || 
               material_.hasSpecularTexture() || 
               material_.hasNormalTexture() || 
               material_.hasHeightTexture() || 
               material_.hasEmissiveTexture();
    }

    size_t MaterialComponent::getTextureCount() const {
        size_t count = 0;
        if (material_.hasDiffuseTexture()) count++;
        if (material_.hasSpecularTexture()) count++;
        if (material_.hasNormalTexture()) count++;
        if (material_.hasHeightTexture()) count++;
        if (material_.hasEmissiveTexture()) count++;
        return count;
    }

    std::shared_ptr<MaterialComponent> MaterialComponent::clone() const {
        auto cloned = std::make_shared<MaterialComponent>();
        cloned->material_ = material_;
        cloned->shader_ = shader_;
        cloned->floatParams_ = floatParams_;
        cloned->vec3Params_ = vec3Params_;
        cloned->vec4Params_ = vec4Params_;
        cloned->intParams_ = intParams_;
        return cloned;
    }

    // === Material Presets ===

    std::shared_ptr<MaterialComponent> MaterialComponent::createColoredMaterial(
        const glm::vec3& diffuseColor,
        const glm::vec3& specularColor,
        float shininess
    ) {
        auto material = std::make_shared<MaterialComponent>();
        material->setDiffuseColor(diffuseColor);
        material->setSpecularColor(specularColor);
        material->setShininess(shininess);
        return material;
    }

    std::shared_ptr<MaterialComponent> MaterialComponent::createTexturedMaterial(
        const std::string& diffusePath,
        const std::string& specularPath,
        const std::string& normalPath
    ) {
        auto material = std::make_shared<MaterialComponent>();
        
        if (!diffusePath.empty()) {
            material->setDiffuseTexture(diffusePath);
        }
        
        if (!specularPath.empty()) {
            material->setSpecularTexture(specularPath);
        }
        
        if (!normalPath.empty()) {
            material->setNormalTexture(normalPath);
        }
        
        return material;
    }

    std::shared_ptr<MaterialComponent> MaterialComponent::createPBRMaterial(
        const std::string& baseColorPath,
        const std::string& metallicRoughnessPath,
        const std::string& normalPath,
        const std::string& emissivePath
    ) {
        auto material = std::make_shared<MaterialComponent>();
        
        if (!baseColorPath.empty()) {
            material->setDiffuseTexture(baseColorPath);
        }
        
        // For PBR, we use specular texture slot for metallic/roughness
        if (!metallicRoughnessPath.empty()) {
            material->setSpecularTexture(metallicRoughnessPath);
        }
        
        if (!normalPath.empty()) {
            material->setNormalTexture(normalPath);
        }
        
        if (!emissivePath.empty()) {
            material->setEmissiveTexture(emissivePath);
        }
        
        // Set appropriate PBR defaults
        material->setShaderParameter("metallicFactor", 1.0f);
        material->setShaderParameter("roughnessFactor", 1.0f);
        material->setShaderParameter("emissiveFactor", glm::vec3(1.0f));
        
        return material;
    }

    // === Protected Methods ===

    void MaterialComponent::onAttach() {
        // Initialize default material properties if needed
        if (material_.shininess <= 0.0f) {
            material_.shininess = 32.0f;
        }
        
        // Set default colors if they're not set
        if (material_.diffuseColor == glm::vec3(0.0f)) {
            material_.diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
        }
        
        if (material_.specularColor == glm::vec3(0.0f)) {
            material_.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
        }
        
        if (material_.ambientColor == glm::vec3(0.0f)) {
            material_.ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
        }
    }

    void MaterialComponent::onDetach() {
        // Clean up resources if needed
        clearShaderParameters();
    }

    // === Private Helper Methods ===

    std::shared_ptr<Texture> MaterialComponent::loadTextureFromPath(const std::string& path) {
        // Check cache first
        auto it = textureCache_.find(path);
        if (it != textureCache_.end()) {
            return it->second;
        }

        // Load new texture
        try {
            auto texture = std::make_shared<Texture>();
            if (texture->loadFromFile(path)) {
                textureCache_[path] = texture;
                return texture;
            }
        } catch (const std::exception& e) {
            std::cerr << "MaterialComponent: Failed to load texture from " << path 
                      << ": " << e.what() << std::endl;
        }

        return nullptr;
    }

    void MaterialComponent::bindTextures() const {
        int textureUnit = 0;

        // Bind diffuse texture
        if (material_.hasDiffuseTexture()) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            material_.diffuse->bind();
            try {
                shader_->setInt("material.diffuse", textureUnit);
                shader_->setBool("material.hasDiffuseTexture", true);
            } catch (...) {
                // Shader might not have these uniforms
            }
            textureUnit++;
        } else {
            try {
                shader_->setBool("material.hasDiffuseTexture", false);
            } catch (...) {}
        }

        // Bind specular texture
        if (material_.hasSpecularTexture()) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            material_.specular->bind();
            try {
                shader_->setInt("material.specular", textureUnit);
                shader_->setBool("material.hasSpecularTexture", true);
            } catch (...) {
                // Shader might not have these uniforms
            }
            textureUnit++;
        } else {
            try {
                shader_->setBool("material.hasSpecularTexture", false);
            } catch (...) {}
        }

        // Bind normal texture
        if (material_.hasNormalTexture()) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            material_.normal->bind();
            try {
                shader_->setInt("material.normal", textureUnit);
                shader_->setBool("material.hasNormalTexture", true);
            } catch (...) {
                // Shader might not have these uniforms
            }
            textureUnit++;
        } else {
            try {
                shader_->setBool("material.hasNormalTexture", false);
            } catch (...) {}
        }

        // Bind height texture
        if (material_.hasHeightTexture()) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            material_.height->bind();
            try {
                shader_->setInt("material.height", textureUnit);
                shader_->setBool("material.hasHeightTexture", true);
            } catch (...) {
                // Shader might not have these uniforms
            }
            textureUnit++;
        } else {
            try {
                shader_->setBool("material.hasHeightTexture", false);
            } catch (...) {}
        }

        // Bind emissive texture
        if (material_.hasEmissiveTexture()) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            material_.emissive->bind();
            try {
                shader_->setInt("material.emissive", textureUnit);
                shader_->setBool("material.hasEmissiveTexture", true);
            } catch (...) {
                // Shader might not have these uniforms
            }
            textureUnit++;
        } else {
            try {
                shader_->setBool("material.hasEmissiveTexture", false);
            } catch (...) {}
        }
    }

    void MaterialComponent::setShaderUniforms() const {
        try {
            // Set color properties
            shader_->setVec3("material.diffuseColor", material_.diffuseColor);
            shader_->setVec3("material.specularColor", material_.specularColor);
            shader_->setVec3("material.ambientColor", material_.ambientColor);
            shader_->setFloat("material.shininess", material_.shininess);
        } catch (const std::exception& e) {
            // Shader might not have material uniforms, which is okay
        }
    }

    void MaterialComponent::applyCustomParameters() const {
        // Apply float parameters
        for (const auto& param : floatParams_) {
            try {
                shader_->setFloat(param.first, param.second);
            } catch (...) {
                // Parameter might not exist in shader
            }
        }

        // Apply vec3 parameters
        for (const auto& param : vec3Params_) {
            try {
                shader_->setVec3(param.first, param.second);
            } catch (...) {
                // Parameter might not exist in shader
            }
        }

        // Apply vec4 parameters
        for (const auto& param : vec4Params_) {
            try {
                shader_->setVec4(param.first, param.second);
            } catch (...) {
                // Parameter might not exist in shader
            }
        }

        // Apply int parameters
        for (const auto& param : intParams_) {
            try {
                shader_->setInt(param.first, param.second);
            } catch (...) {
                // Parameter might not exist in shader
            }
        }
    }

} // namespace IKore
