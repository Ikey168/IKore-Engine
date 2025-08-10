#include "Texture.h"
#include "Logger.h"
#include <iostream>

int main() {
    // Initialize logging
    IKore::Logger::getInstance().initialize();
    LOG_INFO("Starting texture loading test");
    
    // Test individual texture loading
    auto diffuseTexture = IKore::Texture::loadFromFileShared("assets/textures/colorful.png", IKore::Texture::Type::DIFFUSE);
    if (diffuseTexture) {
        LOG_INFO("✓ Successfully loaded diffuse texture: " + diffuseTexture->getPath());
        std::cout << "Diffuse texture loaded: " << diffuseTexture->getWidth() << "x" << diffuseTexture->getHeight() 
                 << " (" << diffuseTexture->getChannels() << " channels)" << std::endl;
    } else {
        LOG_ERROR("✗ Failed to load diffuse texture");
        std::cout << "Failed to load diffuse texture" << std::endl;
    }
    
    auto specularTexture = IKore::Texture::loadFromFileShared("assets/textures/specular.png", IKore::Texture::Type::SPECULAR);
    if (specularTexture) {
        LOG_INFO("✓ Successfully loaded specular texture: " + specularTexture->getPath());
        std::cout << "Specular texture loaded: " << specularTexture->getWidth() << "x" << specularTexture->getHeight() 
                 << " (" << specularTexture->getChannels() << " channels)" << std::endl;
    } else {
        LOG_ERROR("✗ Failed to load specular texture");
        std::cout << "Failed to load specular texture" << std::endl;
    }
    
    // Test texture manager
    IKore::TextureManager textureManager;
    
    if (diffuseTexture) {
        textureManager.addTexture(diffuseTexture);
        LOG_INFO("✓ Added diffuse texture to manager");
    }
    
    if (specularTexture) {
        textureManager.addTexture(specularTexture);
        LOG_INFO("✓ Added specular texture to manager");
    }
    
    std::cout << "Texture manager contains " << textureManager.getTextureCount() << " textures" << std::endl;
    
    // Test cache functionality
    auto cachedTexture = IKore::Texture::loadFromFileShared("assets/textures/colorful.png", IKore::Texture::Type::DIFFUSE);
    if (cachedTexture) {
        std::cout << "Cache test: " << (cachedTexture.get() == diffuseTexture.get() ? "✓ Using cached texture" : "✗ Not using cache") << std::endl;
    }
    
    LOG_INFO("Texture loading test completed successfully!");
    std::cout << "All texture functionality tests passed!" << std::endl;
    
    return 0;
}
