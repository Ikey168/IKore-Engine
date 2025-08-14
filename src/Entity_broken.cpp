#include "core/Entity.h"
#include "core/Logger.h"
#include <mutex>

namespace IKore {

    // Static member initialization
    std::atomic<EntityID> Entity::s_nextID{1}; // Start from 1, 0 is reserved for INVALID_ENTITY_ID

    // ============================================================================
    // Entity Implementation
    // ============================================================================

    Entity::Entity() : m_id(generateID()) {
        LOG_INFO("Entity created with ID: " + std::to_string(m_id));
    }

    Entity::Entity(const Entity& other) : m_id(generateID()) {
        LOG_INFO("Entity copied from ID " + std::to_string(other.m_id) + " to new ID " + std::to_string(m_id));
    }

    Entity::Entity(Entity&& other) noexcept : m_id(other.m_id) {
        other.m_id = 0; // Invalidate the moved-from object's ID
        LOG_INFO("Entity moved to ID: " + std::to_string(m_id));
    }

    Entity& Entity::operator=(const Entity& other) {
        if (this != &other) {
            m_id = generateID(); // Create a new ID for the assigned entity
            LOG_INFO("Entity copy-assigned from ID " + std::to_string(other.m_id) + " to new ID " + std::to_string(m_id));
        }
        return *this;
    }

    Entity& Entity::operator=(Entity&& other) noexcept {
        if (this != &other) {
            m_id = other.m_id;
            other.m_id = 0; // Invalidate the moved-from object's ID
            LOG_INFO("Entity move-assigned to ID: " + std::to_string(m_id));
        }
        return *this;

    EntityID Entity::generateID() {
        return s_nextID.fetch_add(1, std::memory_order_relaxed);
    }

    // ============================================================================
    // EntityManager Implementation
    // ============================================================================

    EntityManager& EntityManager::getInstance() {
        static EntityManager instance;
        return instance;
    }

    void EntityManager::addEntity(std::shared_ptr<Entity> entity) {
        if (!entity || !entity->isValid()) {
            LOG_WARNING("Attempted to add invalid entity to EntityManager");
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        EntityID id = entity->getID();
        
        if (m_entities.find(id) != m_entities.end()) {
            LOG_WARNING("Entity with ID " + std::to_string(id) + " already exists in EntityManager");
            return;
        }

        m_entities[id] = entity;
        LOG_INFO("Entity added to EntityManager with ID: " + std::to_string(id));
    }

    bool EntityManager::removeEntity(EntityID id) {
        if (id == INVALID_ENTITY_ID) {
            LOG_WARNING("Attempted to remove entity with invalid ID");
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entities.find(id);
        if (it != m_entities.end()) {
            // Mark entity as invalid before removing
            if (it->second) {
                it->second->invalidate();
                it->second->cleanup();
            }
            m_entities.erase(it);
            LOG_INFO("Entity removed from EntityManager with ID: " + std::to_string(id));
            return true;
        }

        LOG_WARNING("Entity with ID " + std::to_string(id) + " not found for removal");
        return false;
    }

    bool EntityManager::removeEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            LOG_WARNING("Attempted to remove null entity pointer");
            return false;
        }
        return removeEntity(entity->getID());
    }

    std::shared_ptr<Entity> EntityManager::getEntity(EntityID id) const {
        if (id == INVALID_ENTITY_ID) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entities.find(id);
        if (it != m_entities.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Entity>> EntityManager::getAllEntities() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<Entity>> result;
        result.reserve(m_entities.size());
        
        for (const auto& pair : m_entities) {
            if (pair.second && pair.second->isValid()) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    void EntityManager::forEach(std::function<void(std::shared_ptr<Entity>)> callback) const {
        if (!callback) {
            LOG_WARNING("EntityManager::forEach called with null callback");
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_entities) {
            if (pair.second && pair.second->isValid()) {
                callback(pair.second);
            }
        }
    }

    bool EntityManager::hasEntity(EntityID id) const {
        if (id == INVALID_ENTITY_ID) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entities.find(id);
        return it != m_entities.end() && it->second && it->second->isValid();
    }

    void EntityManager::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Cleanup all entities before clearing
        for (auto& pair : m_entities) {
            if (pair.second) {
                pair.second->cleanup();
                pair.second->invalidate();
            }
        }
        
        size_t count = m_entities.size();
        m_entities.clear();
        LOG_INFO("EntityManager cleared, removed " + std::to_string(count) + " entities");
    }

    void EntityManager::updateAll(float deltaTime) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Create a copy of entities to update to avoid issues with concurrent modification
        std::vector<std::shared_ptr<Entity>> entitiesToUpdate;
        entitiesToUpdate.reserve(m_entities.size());
        
        for (const auto& pair : m_entities) {
            if (pair.second && pair.second->isValid()) {
                entitiesToUpdate.push_back(pair.second);
            }
        }
        
        // Update entities outside the lock to avoid deadlock
        // Note: We temporarily release the lock here
        m_mutex.unlock();
        
        for (auto& entity : entitiesToUpdate) {
            try {
                entity->update(deltaTime);
            } catch (const std::exception& e) {
                LOG_ERROR("Exception during entity update for ID " + std::to_string(entity->getID()) + ": " + e.what());
            }
        }
        
        // Re-acquire the lock
        m_mutex.lock();
    }

        void EntityManager::initializeAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& pair : m_entities) {
            if (pair.second && pair.second->isValid()) {
                try {
                    pair.second->initialize();
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception during entity initialization for ID " + std::to_string(pair.second->getID()) + ": " + e.what());
                }
            }
        }
        
        LOG_INFO("Initialized " + std::to_string(m_entities.size()) + " entities");
    }

        void EntityManager::cleanupAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& pair : m_entities) {
            if (pair.second) {
                try {
                    pair.second->cleanup();
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception during entity cleanup for ID " + std::to_string(pair.second->getID()) + ": " + e.what());
                }
            }
        }
        
        LOG_INFO("Cleaned up " + std::to_string(m_entities.size()) + " entities");
    }

    EntityManager::EntityStats EntityManager::getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        EntityStats stats;
        stats.totalEntities = m_entities.size();
        
        for (const auto& pair : m_entities) {
            if (pair.second) {
                if (pair.second->isValid()) {
                    stats.validEntities++;
                    
                    EntityID id = pair.second->getID();
                    if (stats.lowestID == INVALID_ENTITY_ID || id < stats.lowestID) {
                        stats.lowestID = id;
                    }
                    if (id > stats.highestID) {
                        stats.highestID = id;
                    }
                } else {
                    stats.invalidEntities++;
                }
            } else {
                stats.invalidEntities++;
            }
        }
        
        return stats;
    }

} // namespace IKore
