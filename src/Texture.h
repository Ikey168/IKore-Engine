#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace IKore {

class Texture {
public:
    enum class Type {
        DIFFUSE,
        SPECULAR,
        NORMAL,
        HEIGHT,
        EMISSIVE
    };

    enum class Format {
        RGB,
        RGBA,
        AUTO  // Automatically detect format from image
    };

    enum class WrapMode {
        REPEAT = GL_REPEAT,
        MIRRORED_REPEAT = GL_MIRRORED_REPEAT,
        CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
        CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER
    };

    enum class FilterMode {
        NEAREST = GL_NEAREST,
        LINEAR = GL_LINEAR,
        NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST,
        LINEAR_MIPMAP_NEAREST = GL_LINEAR_MIPMAP_NEAREST,
        NEAREST_MIPMAP_LINEAR = GL_NEAREST_MIPMAP_LINEAR,
        LINEAR_MIPMAP_LINEAR = GL_LINEAR_MIPMAP_LINEAR
    };

private:
    GLuint m_textureID;
    Type m_type;
    std::string m_path;
    int m_width;
    int m_height;
    int m_channels;
    bool m_isLoaded;

    // Static cache for loaded textures
    static std::unordered_map<std::string, std::weak_ptr<Texture>> s_textureCache;

public:
    Texture();
    ~Texture();

    // Delete copy constructor and assignment operator
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Move constructor and assignment operator
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Load texture from file
    bool loadFromFile(const std::string& path, Type type = Type::DIFFUSE, 
                     Format format = Format::AUTO, bool generateMips = true);

    // Load texture from memory
    bool loadFromMemory(const unsigned char* data, int width, int height, 
                       int channels, Type type = Type::DIFFUSE, 
                       Format format = Format::AUTO, bool generateMips = true);

    // Cached texture loading - returns shared texture if already loaded
    static std::shared_ptr<Texture> loadFromFileShared(const std::string& path, 
                                                       Type type = Type::DIFFUSE,
                                                       Format format = Format::AUTO, 
                                                       bool generateMips = true);

    // Bind texture to specified texture unit
    void bind(GLuint textureUnit = 0) const;
    void unbind() const;

    // Texture parameters
    void setWrapMode(WrapMode wrapS, WrapMode wrapT);
    void setFilterMode(FilterMode minFilter, FilterMode magFilter);
    void setBorderColor(float r, float g, float b, float a);

    // Getters
    GLuint getID() const { return m_textureID; }
    Type getType() const { return m_type; }
    const std::string& getPath() const { return m_path; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getChannels() const { return m_channels; }
    bool isLoaded() const { return m_isLoaded; }

    // Static utility functions
    static void clearCache();
    static std::string typeToString(Type type);
    static Type stringToType(const std::string& typeStr);

private:
    void cleanup();
    GLenum getGLFormat(Format format, int channels) const;
    GLenum getGLInternalFormat(Format format, int channels) const;
};

// Texture manager for handling multiple textures per object
class TextureManager {
public:
    struct TextureSlot {
        std::shared_ptr<Texture> texture;
        Texture::Type type;
        GLuint unit;
        std::string uniformName;
    };

private:
    std::vector<TextureSlot> m_textures;
    GLuint m_nextUnit;

public:
    TextureManager();
    
    // Add texture to manager
    bool addTexture(std::shared_ptr<Texture> texture, const std::string& uniformName = "");
    
    // Remove texture by index or type
    void removeTexture(size_t index);
    void removeTextureByType(Texture::Type type);
    
    // Bind all textures and set uniforms
    void bindAll(GLuint shaderProgram) const;
    void unbindAll() const;
    
    // Getters
    size_t getTextureCount() const { return m_textures.size(); }
    const TextureSlot& getTexture(size_t index) const { return m_textures[index]; }
    std::shared_ptr<Texture> getTextureByType(Texture::Type type) const;
    
    // Clear all textures
    void clear();

private:
    std::string getDefaultUniformName(Texture::Type type) const;
};

} // namespace IKore
