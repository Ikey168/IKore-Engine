#pragma once

#include "../Component.h"

namespace IKore {
    class RenderableComponent : public Component {
    public:
        RenderableComponent() = default;
        virtual ~RenderableComponent() = default;
        
        // Rendering properties
        bool visible = true;
        std::string meshPath;
        std::string texturePath;
    };
}
