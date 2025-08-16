#pragma once

#include "../Component.h"
#include "../../Model.h"
#include "../../Shader.h"
#include "../../Texture.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace IKore {

    /**
     * @brief Component for managing material properties and shader assignments
     * 
     * The MaterialComponent provides a flexible system for managing material properties
     * including textures, colors, and shader parameters. It supports:
     * - Dynamic material property updates
     * - Multiple texture types (diffuse, specular, normal, height, emissive)
     * - Shader reference management
     * - Custom shader parameter binding
     * - Runtime material switching
     */
    class MaterialComponent : public Component {
    public:
        MaterialComponent() = default;
        virtual ~MaterialComponent() = default;

        // === Core Material Properties ===

        /**
         * @brief Set the base material properties
         * @param material Material struct with textures and color properties
         */
        void setMaterial(const Material& material);

        /**
         * @brief Get the current material properties
         * @return Reference to the internal material
         */
        const Material& getMaterial() const { return material_; }

        /**
         * @brief Get mutable reference to material for direct modification
         * @return Mutable reference to the internal material
         */
        Material& getMaterial() { return material_; }

        // === Texture Management ===

        /**
         * @brief Set diffuse texture from file path
         * @param path Path to the diffuse texture file
         * @return True if texture loaded successfully
         */
        bool setDiffuseTexture(const std::string& path);

        /**
         * @brief Set diffuse texture from existing texture object
         * @param texture Shared pointer to texture
         */
        void setDiffuseTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Set specular texture from file path
         * @param path Path to the specular texture file
         * @return True if texture loaded successfully
         */
        bool setSpecularTexture(const std::string& path);

        /**
         * @brief Set specular texture from existing texture object
         * @param texture Shared pointer to texture
         */
        void setSpecularTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Set normal map texture from file path
         * @param path Path to the normal texture file
         * @return True if texture loaded successfully
         */
        bool setNormalTexture(const std::string& path);

        /**
         * @brief Set normal map texture from existing texture object
         * @param texture Shared pointer to texture
         */
        void setNormalTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Set height map texture from file path
         * @param path Path to the height texture file
         * @return True if texture loaded successfully
         */
        bool setHeightTexture(const std::string& path);

        /**
         * @brief Set height map texture from existing texture object
         * @param texture Shared pointer to texture
         */
        void setHeightTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Set emissive texture from file path
         * @param path Path to the emissive texture file
         * @return True if texture loaded successfully
         */
        bool setEmissiveTexture(const std::string& path);

        /**
         * @brief Set emissive texture from existing texture object
         * @param texture Shared pointer to texture
         */
        void setEmissiveTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Clear a specific texture type
         * @param textureType Type of texture to clear
         */
        void clearTexture(const std::string& textureType);

        /**
         * @brief Clear all textures
         */
        void clearAllTextures();

        // === Color Properties ===

        /**
         * @brief Set diffuse color
         * @param color RGB color values (0.0 - 1.0)
         */
        void setDiffuseColor(const glm::vec3& color) { material_.diffuseColor = color; }

        /**
         * @brief Set specular color
         * @param color RGB color values (0.0 - 1.0)
         */
        void setSpecularColor(const glm::vec3& color) { material_.specularColor = color; }

        /**
         * @brief Set ambient color
         * @param color RGB color values (0.0 - 1.0)
         */
        void setAmbientColor(const glm::vec3& color) { material_.ambientColor = color; }

        /**
         * @brief Set material shininess
         * @param shininess Shininess value (typically 1.0 - 256.0)
         */
        void setShininess(float shininess) { material_.shininess = shininess; }

        // === Shader Management ===

        /**
         * @brief Set the shader for this material
         * @param shader Shared pointer to shader program
         */
        void setShader(std::shared_ptr<Shader> shader) { shader_ = shader; }

        /**
         * @brief Load shader from vertex and fragment files
         * @param vertexPath Path to vertex shader file
         * @param fragmentPath Path to fragment shader file
         * @return True if shader loaded successfully
         */
        bool loadShader(const std::string& vertexPath, const std::string& fragmentPath);

        /**
         * @brief Get the current shader
         * @return Shared pointer to shader (may be null)
         */
        std::shared_ptr<Shader> getShader() const { return shader_; }

        /**
         * @brief Check if a valid shader is assigned
         * @return True if shader is available
         */
        bool hasShader() const { return shader_ != nullptr; }

        // === Custom Shader Parameters ===

        /**
         * @brief Set a custom float parameter for the shader
         * @param name Parameter name in shader
         * @param value Float value
         */
        void setShaderParameter(const std::string& name, float value);

        /**
         * @brief Set a custom vec3 parameter for the shader
         * @param name Parameter name in shader
         * @param value Vector3 value
         */
        void setShaderParameter(const std::string& name, const glm::vec3& value);

        /**
         * @brief Set a custom vec4 parameter for the shader
         * @param name Parameter name in shader
         * @param value Vector4 value
         */
        void setShaderParameter(const std::string& name, const glm::vec4& value);

        /**
         * @brief Set a custom integer parameter for the shader
         * @param name Parameter name in shader
         * @param value Integer value
         */
        void setShaderParameter(const std::string& name, int value);

        /**
         * @brief Remove a custom shader parameter
         * @param name Parameter name to remove
         */
        void removeShaderParameter(const std::string& name);

        /**
         * @brief Clear all custom shader parameters
         */
        void clearShaderParameters();

        // === Rendering Integration ===

        /**
         * @brief Apply this material to rendering pipeline
         * Binds textures and sets shader uniforms
         */
        void apply() const;

        /**
         * @brief Apply material with custom model matrix
         * @param modelMatrix Transformation matrix for the object
         */
        void apply(const glm::mat4& modelMatrix) const;

        /**
         * @brief Unbind material resources
         */
        void unbind() const;

        // === Utility Functions ===

        /**
         * @brief Check if material has any textures
         * @return True if at least one texture is loaded
         */
        bool hasTextures() const;

        /**
         * @brief Get texture count
         * @return Number of loaded textures
         */
        size_t getTextureCount() const;

        /**
         * @brief Create a copy of this material component
         * @return New MaterialComponent with same properties
         */
        std::shared_ptr<MaterialComponent> clone() const;

        // === Material Presets ===

        /**
         * @brief Create a basic colored material
         * @param diffuseColor Primary color
         * @param specularColor Specular highlight color
         * @param shininess Material shininess
         * @return MaterialComponent with color properties
         */
        static std::shared_ptr<MaterialComponent> createColoredMaterial(
            const glm::vec3& diffuseColor,
            const glm::vec3& specularColor = glm::vec3(0.5f),
            float shininess = 32.0f
        );

        /**
         * @brief Create a textured material with diffuse texture
         * @param diffusePath Path to diffuse texture
         * @param specularPath Optional path to specular texture
         * @param normalPath Optional path to normal map
         * @return MaterialComponent with loaded textures
         */
        static std::shared_ptr<MaterialComponent> createTexturedMaterial(
            const std::string& diffusePath,
            const std::string& specularPath = "",
            const std::string& normalPath = ""
        );

        /**
         * @brief Create a PBR (Physically Based Rendering) material
         * @param baseColorPath Path to base color (diffuse) texture
         * @param metallicRoughnessPath Path to metallic/roughness texture
         * @param normalPath Path to normal map
         * @param emissivePath Optional path to emissive texture
         * @return MaterialComponent configured for PBR rendering
         */
        static std::shared_ptr<MaterialComponent> createPBRMaterial(
            const std::string& baseColorPath,
            const std::string& metallicRoughnessPath = "",
            const std::string& normalPath = "",
            const std::string& emissivePath = ""
        );

    protected:
        void onAttach() override;
        void onDetach() override;

    private:
        /// Core material properties
        Material material_;

        /// Shader program reference
        std::shared_ptr<Shader> shader_;

        /// Custom shader parameters
        std::unordered_map<std::string, float> floatParams_;
        std::unordered_map<std::string, glm::vec3> vec3Params_;
        std::unordered_map<std::string, glm::vec4> vec4Params_;
        std::unordered_map<std::string, int> intParams_;

        /// Texture cache for string-based loading
        static std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache_;

        /// Helper methods for texture loading
        std::shared_ptr<Texture> loadTextureFromPath(const std::string& path);
        void bindTextures() const;
        void setShaderUniforms() const;
        void applyCustomParameters() const;
    };

} // namespace IKore
