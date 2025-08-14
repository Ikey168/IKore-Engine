#include "Entity.h"
#include "Component.h"
#include <atomic>

namespace IKore {
    static std::atomic<EntityID> s_nextID{1};

    Entity::Entity() : m_id(generateUniqueID()) {}

    EntityID Entity::generateUniqueID() {
        return s_nextID.fetch_add(1);
    }
}
