#pragma once

#include "../Component.h"
#include <glm/glm.hpp>

namespace IKore {
    class TransformComponent : public Component {
    public:
        TransformComponent() = default;
        virtual ~TransformComponent() = default;
        
        // Transform properties
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};
    };
}
