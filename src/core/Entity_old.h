#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <glm/glm.hpp>

// Forward declaration
class Component;

namespace IKore {
    using EntityID = uint64_t;
    constexpr EntityID INVALID_ENTITY_ID = 0;

    class Entity : public std::enable_shared_from_this<Entity> {
    public:
        Entity();
        virtual ~Entity() = default;
        
        EntityID getID() const { return m_id; }
        bool isValid() const { return m_id != INVALID_ENTITY_ID; }
        
        // Component System Methods
        template<typename T, typename... Args>
        std::shared_ptr<T> addComponent(Args&&... args) {
            std::type_index typeIndex(typeid(T));
            
            auto it = m_components.find(typeIndex);
            if (it != m_components.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            
            auto component = std::make_shared<T>(std::forward<Args>(args)...);
            component->setEntity(weak_from_this());
            
            m_components[typeIndex] = component;
            return component;
        }

        template<typename T>
        void removeComponent() {
            std::type_index typeIndex(typeid(T));
            auto it = m_components.find(typeIndex);
            if (it != m_components.end()) {
                it->second->onDetach();
                m_components.erase(it);
            }
        }

        template<typename T>
        std::shared_ptr<T> getComponent() {
            std::type_index typeIndex(typeid(T));
            auto it = m_components.find(typeIndex);
            if (it != m_components.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            return nullptr;
        }

        template<typename T>
        bool hasComponent() const {
            std::type_index typeIndex(typeid(T));
            return m_components.find(typeIndex) != m_components.end();
        }

    private:
        EntityID m_id;
        std::unordered_map<std::type_index, std::shared_ptr<Component>> m_components;
        
        static EntityID generateUniqueID();
    };
}
