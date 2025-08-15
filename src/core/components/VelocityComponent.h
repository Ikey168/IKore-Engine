#pragma once

#include "../Component.h"
#include <glm/glm.hpp>

namespace IKore {
    class VelocityComponent : public Component {
    public:
        VelocityComponent() = default;
        virtual ~VelocityComponent() = default;
        
        // Velocity properties
        glm::vec3 velocity{0.0f};
        float maxSpeed = 10.0f;
        
        void integrate(float deltaTime) {
            // Basic integration for movement
            if (auto entity = getEntity().lock()) {
                // Integration logic would go here
            }
        }
    };
}
