#include "Texture.h"
#include "Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <algorithm>

namespace IKore {

// Static member initialization
std::unordered_map<std::string, std::weak_ptr<Texture>> Texture::s_textureCache;

Texture::Texture() 
    : m_textureID(0), m_type(Type::DIFFUSE), m_width(0), m_height(0), 
      m_channels(0), m_isLoaded(false) {
}

Texture::~Texture() {
    cleanup();
}

Texture::Texture(Texture&& other) noexcept 
    : m_textureID(other.m_textureID), m_type(other.m_type), m_path(std::move(other.m_path)),
      m_width(other.m_width), m_height(other.m_height), m_channels(other.m_channels),
      m_isLoaded(other.m_isLoaded) {
    other.m_textureID = 0;
    other.m_isLoaded = false;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        m_textureID = other.m_textureID;
        m_type = other.m_type;
        m_path = std::move(other.m_path);
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_isLoaded = other.m_isLoaded;
        
        other.m_textureID = 0;
        other.m_isLoaded = false;
    }
    return *this;
}

bool Texture::loadFromFile(const std::string& path, Type type, Format format, bool generateMips) {
    LOG_INFO("Loading texture from file: " + path);
    
    // Set texture properties
    m_type = type;
    m_path = path;
    
    // Load image data
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &m_width, &m_height, &m_channels, 0);
    
    if (!data) {
        LOG_ERROR("Failed to load texture: " + path + " - " + std::string(stbi_failure_reason()));
        return false;
    }
    
    bool result = loadFromMemory(data, m_width, m_height, m_channels, type, format, generateMips);
    
    stbi_image_free(data);
    
    if (result) {
        LOG_INFO("Successfully loaded texture: " + path + 
                " (" + std::to_string(m_width) + "x" + std::to_string(m_height) + 
                ", " + std::to_string(m_channels) + " channels)");
    }
    
    return result;
}

bool Texture::loadFromMemory(const unsigned char* data, int width, int height, 
                            int channels, Type type, Format format, bool generateMips) {
    if (!data || width <= 0 || height <= 0 || channels <= 0) {
        LOG_ERROR("Invalid texture data parameters");
        return false;
    }
    
    // Clean up previous texture if exists
    cleanup();
    
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_type = type;
    
    // Generate OpenGL texture
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    // Determine formats
    GLenum glFormat = getGLFormat(format, channels);
    GLenum glInternalFormat = getGLInternalFormat(format, channels);
    
    if (glFormat == 0 || glInternalFormat == 0) {
        LOG_ERROR("Unsupported texture format");
        cleanup();
        return false;
    }
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, data);
    
    // Generate mipmaps if requested
    if (generateMips) {
        glGenerateMipmap(GL_TEXTURE_2D);
        // Set default filtering for mipmaps
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        // Set default filtering without mipmaps
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    // Set default wrap mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    m_isLoaded = true;
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR("OpenGL error while creating texture: " + std::to_string(error));
        cleanup();
        return false;
    }
    
    return true;
}

std::shared_ptr<Texture> Texture::loadFromFileShared(const std::string& path, Type type, 
                                                     Format format, bool generateMips) {
    // Check cache first
    auto it = s_textureCache.find(path);
    if (it != s_textureCache.end()) {
        if (auto existingTexture = it->second.lock()) {
            LOG_INFO("Using cached texture: " + path);
            return existingTexture;
        } else {
            // Remove expired weak_ptr
            s_textureCache.erase(it);
        }
    }
    
    // Create new texture
    auto texture = std::make_shared<Texture>();
    if (!texture->loadFromFile(path, type, format, generateMips)) {
        return nullptr;
    }
    
    // Add to cache
    s_textureCache[path] = texture;
    
    return texture;
}

void Texture::bind(GLuint textureUnit) const {
    if (m_isLoaded) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
    }
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setWrapMode(WrapMode wrapS, WrapMode wrapT) {
    if (m_isLoaded) {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture::setFilterMode(FilterMode minFilter, FilterMode magFilter) {
    if (m_isLoaded) {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture::setBorderColor(float r, float g, float b, float a) {
    if (m_isLoaded) {
        float borderColor[] = {r, g, b, a};
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture::clearCache() {
    s_textureCache.clear();
}

std::string Texture::typeToString(Type type) {
    switch (type) {
        case Type::DIFFUSE: return "diffuse";
        case Type::SPECULAR: return "specular";
        case Type::NORMAL: return "normal";
        case Type::HEIGHT: return "height";
        case Type::EMISSIVE: return "emissive";
        default: return "unknown";
    }
}

Texture::Type Texture::stringToType(const std::string& typeStr) {
    if (typeStr == "diffuse") return Type::DIFFUSE;
    if (typeStr == "specular") return Type::SPECULAR;
    if (typeStr == "normal") return Type::NORMAL;
    if (typeStr == "height") return Type::HEIGHT;
    if (typeStr == "emissive") return Type::EMISSIVE;
    return Type::DIFFUSE; // Default
}

void Texture::cleanup() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    m_isLoaded = false;
}

GLenum Texture::getGLFormat(Format format, int channels) const {
    if (format == Format::AUTO) {
        switch (channels) {
            case 1: return GL_RED;
            case 3: return GL_RGB;
            case 4: return GL_RGBA;
            default: return 0;
        }
    } else {
        switch (format) {
            case Format::RGB: return GL_RGB;
            case Format::RGBA: return GL_RGBA;
            default: return 0;
        }
    }
}

GLenum Texture::getGLInternalFormat(Format format, int channels) const {
    if (format == Format::AUTO) {
        switch (channels) {
            case 1: return GL_RED;
            case 3: return GL_RGB;
            case 4: return GL_RGBA;
            default: return 0;
        }
    } else {
        switch (format) {
            case Format::RGB: return GL_RGB;
            case Format::RGBA: return GL_RGBA;
            default: return 0;
        }
    }
}

// TextureManager implementation
TextureManager::TextureManager() : m_nextUnit(0) {
}

bool TextureManager::addTexture(std::shared_ptr<Texture> texture, const std::string& uniformName) {
    if (!texture || !texture->isLoaded()) {
        LOG_ERROR("Cannot add invalid or unloaded texture to TextureManager");
        return false;
    }
    
    TextureSlot slot;
    slot.texture = texture;
    slot.type = texture->getType();
    slot.unit = m_nextUnit++;
    slot.uniformName = uniformName.empty() ? getDefaultUniformName(texture->getType()) : uniformName;
    
    m_textures.push_back(slot);
    
    LOG_INFO("Added texture to manager: " + texture->getPath() + 
             " (unit: " + std::to_string(slot.unit) + 
             ", uniform: " + slot.uniformName + ")");
    
    return true;
}

void TextureManager::removeTexture(size_t index) {
    if (index < m_textures.size()) {
        LOG_INFO("Removing texture from manager: " + m_textures[index].texture->getPath());
        m_textures.erase(m_textures.begin() + index);
        
        // Reassign texture units
        for (size_t i = 0; i < m_textures.size(); ++i) {
            m_textures[i].unit = static_cast<GLuint>(i);
        }
        m_nextUnit = static_cast<GLuint>(m_textures.size());
    }
}

void TextureManager::removeTextureByType(Texture::Type type) {
    auto it = std::remove_if(m_textures.begin(), m_textures.end(),
        [type](const TextureSlot& slot) { return slot.type == type; });
    
    if (it != m_textures.end()) {
        m_textures.erase(it, m_textures.end());
        
        // Reassign texture units
        for (size_t i = 0; i < m_textures.size(); ++i) {
            m_textures[i].unit = static_cast<GLuint>(i);
        }
        m_nextUnit = static_cast<GLuint>(m_textures.size());
    }
}

void TextureManager::bindAll(GLuint shaderProgram) const {
    for (const auto& slot : m_textures) {
        slot.texture->bind(slot.unit);
        
        // Set uniform
        GLint location = glGetUniformLocation(shaderProgram, slot.uniformName.c_str());
        if (location != -1) {
            glUniform1i(location, slot.unit);
        }
    }
}

void TextureManager::unbindAll() const {
    for (GLuint i = 0; i < m_nextUnit; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0); // Reset to default
}

std::shared_ptr<Texture> TextureManager::getTextureByType(Texture::Type type) const {
    for (const auto& slot : m_textures) {
        if (slot.type == type) {
            return slot.texture;
        }
    }
    return nullptr;
}

void TextureManager::clear() {
    LOG_INFO("Clearing all textures from manager");
    m_textures.clear();
    m_nextUnit = 0;
}

std::string TextureManager::getDefaultUniformName(Texture::Type type) const {
    switch (type) {
        case Texture::Type::DIFFUSE: return "material.diffuse";
        case Texture::Type::SPECULAR: return "material.specular";
        case Texture::Type::NORMAL: return "material.normal";
        case Texture::Type::HEIGHT: return "material.height";
        case Texture::Type::EMISSIVE: return "material.emissive";
        default: return "material.texture";
    }
}

} // namespace IKore
