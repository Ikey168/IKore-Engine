#pragma once
#include "EntityTypes.h"
#include "Serialization.h"

namespace IKore {
    inline void registerAllEntityTypes() {
        EntityRegistry::registerEntityType<GameObject>("GameObject");
        EntityRegistry::registerEntityType<LightEntity>("LightEntity");
        EntityRegistry::registerEntityType<CameraEntity>("CameraEntity");
        EntityRegistry::registerEntityType<TestEntity>("TestEntity");
    }
}
