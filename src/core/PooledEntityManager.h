#pragma once

#include "Entity.h"
#include "EntityPool.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "Entity.h"
#include "EntityPool.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace IKore {

    /**
     * @brief Enhanced EntityManager with pooling support
     * 
     * This enhanced EntityManager integrates with the EntityPool system
     * to provide high-performance entity management with automatic pooling.
     */
    class PooledEntityManager {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the PooledEntityManager instance
         */
        static PooledEntityManager& getInstance();

        /**
         * @brief Create a pooled entity of the specified type
         * @tparam T Entity type (must inherit from Entity)
         * @tparam Args Constructor argument types
         * @param poolName Optional pool name (uses default if empty)
         * @param args Constructor arguments
         * @return Shared pointer to the created entity
         */
        template<typename T, typename... Args>
        std::shared_ptr<T> createEntity(const std::string& poolName = "", Args&&... args) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            EntityPool& pool = poolName.empty() ? 
                EntityPoolManager::getInstance().getDefaultPool() :
                EntityPoolManager::getInstance().getPool(poolName);
            
            auto entity = pool.borrowEntity<T>(std::forward<Args>(args)...);
            
            // Add to entity tracking
            addEntity(entity, &pool);
            
            return entity;
        }

        /**
         * @brief Create a pooled entity with RAII wrapper
         * @tparam T Entity type (must inherit from Entity)
         * @tparam Args Constructor argument types
         * @param poolName Optional pool name (uses default if empty)
         * @param args Constructor arguments
         * @return PooledEntity wrapper that automatically returns entity to pool
         */
        template<typename T, typename... Args>
        PooledEntity<T> createPooledEntity(const std::string& poolName = "", Args&&... args) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            EntityPool& pool = poolName.empty() ? 
                EntityPoolManager::getInstance().getDefaultPool() :
                EntityPoolManager::getInstance().getPool(poolName);
            
            auto entity = pool.borrowEntity<T>(std::forward<Args>(args)...);
            addEntity(entity, &pool);
            
            return PooledEntity<T>(entity, &pool);
        }

        /**
         * @brief Destroy a pooled entity (returns it to pool)
         * @param entity Entity to destroy
         * @return True if entity was successfully returned to pool
         */
        bool destroyEntity(std::shared_ptr<Entity> entity);

        /**
         * @brief Destroy a pooled entity by ID
         * @param id Entity ID to destroy
         * @return True if entity was found and destroyed
         */
        bool destroyEntity(EntityID id);

        /**
         * @brief Get an entity by ID
         * @param id The entity ID to find
         * @return Shared pointer to the entity, or nullptr if not found
         */
        std::shared_ptr<Entity> getEntity(EntityID id) const;

        /**
         * @brief Get an entity by ID with specific type
         * @tparam T The expected entity type
         * @param id The entity ID to find
         * @return Shared pointer to the entity cast to type T, or nullptr if not found or wrong type
         */
        template<typename T>
        std::shared_ptr<T> getEntity(EntityID id) const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            auto entity = getEntity(id);
            return std::dynamic_pointer_cast<T>(entity);
        }

        /**
         * @brief Get all managed entities
         * @return Vector of shared pointers to all entities
         */
        std::vector<std::shared_ptr<Entity>> getAllEntities() const;

        /**
         * @brief Get all entities of a specific type
         * @tparam T The entity type to filter by
         * @return Vector of shared pointers to entities of type T
         */
        template<typename T>
        std::vector<std::shared_ptr<T>> getEntitiesOfType() const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            std::vector<std::shared_ptr<T>> result;
            
            for (const auto& pair : m_entities) {
                if (auto typedEntity = std::dynamic_pointer_cast<T>(pair.second.entity)) {
                    result.push_back(typedEntity);
                }
            }
            return result;
        }

        /**
         * @brief Iterate over all entities with a callback
         * @param callback Function to call for each entity
         */
        void forEach(std::function<void(std::shared_ptr<Entity>)> callback) const;

        /**
         * @brief Iterate over entities of a specific type with a callback
         * @tparam T The entity type to filter by
         * @param callback Function to call for each entity of type T
         */
        template<typename T>
        void forEach(std::function<void(std::shared_ptr<T>)> callback) const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_entities) {
                if (auto typedEntity = std::dynamic_pointer_cast<T>(pair.second.entity)) {
                    callback(typedEntity);
                }
            }
        }

        /**
         * @brief Get the number of entities managed
         * @return The total number of entities
         */
        size_t getEntityCount() const;

        /**
         * @brief Check if an entity exists by ID
         * @param id The entity ID to check
         * @return True if the entity exists
         */
        bool hasEntity(EntityID id) const;

        /**
         * @brief Clear all entities (returns them to their pools)
         */
        void clear();

        /**
         * @brief Update all entities
         * @param deltaTime Time elapsed since last update
         */
        void updateAll(float deltaTime);

        /**
         * @brief Initialize all entities
         */
        void initializeAll();

        /**
         * @brief Cleanup and return all entities to their pools
         */
        void cleanupAll();

        /**
         * @brief Get entity and pooling statistics
         */
        struct PooledEntityStats {
            size_t totalEntities;
            size_t entitiesFromDefaultPool;
            size_t entitiesFromNamedPools;
            EntityPool::PoolStats aggregatePoolStats;
            std::unordered_map<std::string, size_t> entitiesByPool;
            
            // Constructor for C++17 compatibility
            PooledEntityStats() : totalEntities(0), entitiesFromDefaultPool(0), 
                                entitiesFromNamedPools(0) {}
        };

        /**
         * @brief Get current entity and pool statistics
         * @return Combined statistics structure
         */
        PooledEntityStats getStats() const;

        /**
         * @brief Force garbage collection - destroy entities marked for deletion
         * @return Number of entities garbage collected
         */
        size_t garbageCollect();

        /**
         * @brief Mark an entity for deletion (will be cleaned up on next GC)
         * @param entity Entity to mark for deletion
         */
        void markForDeletion(std::shared_ptr<Entity> entity);

        /**
         * @brief Mark an entity for deletion by ID
         * @param id Entity ID to mark for deletion
         */
        void markForDeletion(EntityID id);

        /**
         * @brief Set automatic garbage collection interval
         * @param intervalMs Interval in milliseconds (0 = disabled)
         */
        void setAutoGCInterval(uint32_t intervalMs);

        /**
         * @brief Enable or disable automatic garbage collection
         * @param enabled Whether auto GC should be enabled
         */
        void setAutoGCEnabled(bool enabled);

    private:
        PooledEntityManager() = default;
        ~PooledEntityManager();

        // Non-copyable and non-movable
        PooledEntityManager(const PooledEntityManager&) = delete;
        PooledEntityManager& operator=(const PooledEntityManager&) = delete;
        PooledEntityManager(PooledEntityManager&&) = delete;
        PooledEntityManager& operator=(PooledEntityManager&&) = delete;

        struct EntityInfo {
            std::shared_ptr<Entity> entity;
            EntityPool* pool;
            bool markedForDeletion = false;
        };

        std::unordered_map<EntityID, EntityInfo> m_entities;
        mutable std::mutex m_mutex;

        // Garbage collection
        std::vector<EntityID> m_deletionQueue;
        uint32_t m_autoGCInterval = 0; // milliseconds, 0 = disabled
        bool m_autoGCEnabled = false;
        std::chrono::steady_clock::time_point m_lastGC;

        /**
         * @brief Add an entity to tracking
         * @param entity Entity to track
         * @param pool Pool that owns the entity
         */
        void addEntity(std::shared_ptr<Entity> entity, EntityPool* pool);

        /**
         * @brief Remove an entity from tracking
         * @param id Entity ID to remove
         * @return True if entity was removed
         */
        bool removeEntity(EntityID id);

        /**
         * @brief Check if automatic garbage collection should run
         * @return True if auto GC should run
         */
        bool shouldRunAutoGC() const;

        /**
         * @brief Run automatic garbage collection if needed
         */
        void autoGarbageCollect();
    };

    /**
     * @brief Convenience function to get the PooledEntityManager instance
     * @return Reference to the PooledEntityManager singleton
     */
    inline PooledEntityManager& getPooledEntityManager() {
        return PooledEntityManager::getInstance();
    }

} // namespace IKore
