#pragma once
#include <glm/glm.hpp>

namespace IKore {

enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOTLIGHT
};

struct Light {
    LightType type;
    glm::vec3 position;      // For point and spot lights
    glm::vec3 direction;     // For directional and spot lights
    glm::vec3 ambient;       // Ambient component
    glm::vec3 diffuse;       // Diffuse component  
    glm::vec3 specular;      // Specular component
    
    // Attenuation (for point and spot lights)
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    
    // Spotlight specific
    float cutOff = 12.5f;        // Inner cone angle in degrees
    float outerCutOff = 17.5f;   // Outer cone angle in degrees
    
    Light(LightType t) : type(t) {
        ambient = glm::vec3(0.2f, 0.2f, 0.2f);
        diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
        specular = glm::vec3(1.0f, 1.0f, 1.0f);
    }
};

class DirectionalLight : public Light {
public:
    DirectionalLight(const glm::vec3& dir = glm::vec3(-0.2f, -1.0f, -0.3f)) 
        : Light(LightType::DIRECTIONAL) {
        direction = glm::normalize(dir);
    }
    
    void setDirection(const glm::vec3& dir) {
        direction = glm::normalize(dir);
    }
};

class PointLight : public Light {
public:
    PointLight(const glm::vec3& pos = glm::vec3(0.0f, 0.0f, 0.0f))
        : Light(LightType::POINT) {
        position = pos;
    }
    
    void setPosition(const glm::vec3& pos) {
        position = pos;
    }
};

class SpotLight : public Light {
public:
    SpotLight(const glm::vec3& pos = glm::vec3(0.0f, 0.0f, 0.0f),
              const glm::vec3& dir = glm::vec3(0.0f, 0.0f, -1.0f))
        : Light(LightType::SPOTLIGHT) {
        position = pos;
        direction = glm::normalize(dir);
    }
    
    void setPosition(const glm::vec3& pos) {
        position = pos;
    }
    
    void setDirection(const glm::vec3& dir) {
        direction = glm::normalize(dir);
    }
    
    void setCutOff(float inner, float outer) {
        cutOff = inner;
        outerCutOff = outer;
    }
};

}
