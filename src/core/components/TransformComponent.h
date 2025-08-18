#pragma once

#include "../Component.h"
#include <glm/glm.hpp>

namespace IKore {
    class TransformComponent : public Component {
    public:
        TransformComponent() = default;
        virtual ~TransformComponent() = default;
        
        // Transform properties
        void setPosition(const glm::vec3& pos) { position = pos; }
        const glm::vec3& getPosition() const { return position; }
        
        void setRotation(const glm::vec3& rot) { rotation = rot; }
        const glm::vec3& getRotation() const { return rotation; }
        
        void setScale(const glm::vec3& scl) { scale = scl; }
        const glm::vec3& getScale() const { return scale; }
        
    private:
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};
    };
}
