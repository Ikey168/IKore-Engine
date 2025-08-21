#pragma once

#include "Entity.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <queue>
#include <chrono>
#include <string>
#include <unordered_map>

namespace IKore {

    /**
     * @brief Entity Pool for performance optimization
     * 
     * This class manages a pool of reusable entities to reduce the overhead
     * of entity creation and destruction. It maintains pools for different
     * entity types and provides efficient entity reuse mechanisms.
     */
    class EntityPool {
    public:
        /**
         * @brief Pool configuration parameters
         */
        struct PoolConfig {
            size_t initialSize;
            size_t maxSize;
            size_t growthSize;
            bool preAllocate;
            bool enableShrinking;
            float shrinkThreshold;
            std::chrono::milliseconds shrinkInterval;
            
            // Constructor with default values for C++17 compatibility
            PoolConfig(size_t initSize = 50, size_t maxSz = 1000, size_t growthSz = 25,
                      bool preAlloc = true, bool enableShrink = true, float shrinkThresh = 0.75f,
                      std::chrono::milliseconds shrinkInt = std::chrono::milliseconds{30000})
                : initialSize(initSize), maxSize(maxSz), growthSize(growthSz),
                  preAllocate(preAlloc), enableShrinking(enableShrink), 
                  shrinkThreshold(shrinkThresh), shrinkInterval(shrinkInt) {}
        };

        /**
         * @brief Pool statistics for monitoring performance
         */
        struct PoolStats {
            size_t totalEntities;
            size_t activeEntities;
            size_t availableEntities;
            size_t totalBorrows;
            size_t totalReturns;
            size_t totalCreations;
            size_t totalDestructions;
            size_t poolHits;
            size_t poolMisses;
            float hitRatio;
            std::chrono::steady_clock::time_point lastShrink;
            
            // Constructor for C++17 compatibility
            PoolStats() : totalEntities(0), activeEntities(0), availableEntities(0),
                         totalBorrows(0), totalReturns(0), totalCreations(0),
                         totalDestructions(0), poolHits(0), poolMisses(0),
                         hitRatio(0.0f), lastShrink(std::chrono::steady_clock::now()) {}
        };

        /**
         * @brief Constructor with configuration
         * @param config Pool configuration parameters
         */
        explicit EntityPool(const PoolConfig& config = PoolConfig{});

        /**
         * @brief Destructor
         */
        ~EntityPool();

        /**
         * @brief Get or create an entity of specified type
         * @tparam T Entity type (must inherit from Entity)
         * @tparam Args Constructor argument types
         * @param args Constructor arguments for new entities
         * @return Shared pointer to entity
         */
        template<typename T, typename... Args>
        std::shared_ptr<T> borrowEntity(Args&&... args) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Try to get entity from type-specific pool
            auto typeIndex = std::type_index(typeid(T));
            auto& typePool = m_typePools[typeIndex];
            
            std::shared_ptr<T> entity;
            
            if (!typePool.empty()) {
                // Reuse existing entity from pool
                auto baseEntity = typePool.back();
                typePool.pop_back();
                
                entity = std::static_pointer_cast<T>(baseEntity);
                resetEntity(entity);
                
                m_stats.poolHits++;
                m_stats.availableEntities--;
            } else {
                // Create new entity
                entity = std::make_shared<T>(std::forward<Args>(args)...);
                m_stats.poolMisses++;
                m_stats.totalCreations++;
            }
            
            // Track active entity
            m_activeEntities.insert(entity);
            m_stats.activeEntities++;
            m_stats.totalBorrows++;
            
            updateHitRatio();
            
            return entity;
        }

        /**
         * @brief Return an entity to the pool for reuse
         * @tparam T Entity type
         * @param entity Entity to return to pool
         * @return True if entity was returned successfully
         */
        template<typename T>
        bool returnEntity(std::shared_ptr<T> entity) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            if (!entity) {
                return false;
            }
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Check if entity is actually active
            auto it = m_activeEntities.find(entity);
            if (it == m_activeEntities.end()) {
                return false; // Entity not tracked by this pool
            }
            
            // Remove from active entities
            m_activeEntities.erase(it);
            m_stats.activeEntities--;
            
            // Clean up entity before returning to pool
            cleanupEntity(entity);
            
            // Check pool size limits
            auto typeIndex = std::type_index(typeid(T));
            auto& typePool = m_typePools[typeIndex];
            
            if (m_config.maxSize == 0 || typePool.size() < m_config.maxSize) {
                // Return to pool
                typePool.push_back(entity);
                m_stats.availableEntities++;
                m_stats.totalReturns++;
            } else {
                // Pool is full, entity will be destroyed
                m_stats.totalDestructions++;
            }
            
            return true;
        }

        /**
         * @brief Pre-allocate entities of a specific type
         * @tparam T Entity type
         * @param count Number of entities to pre-allocate
         */
        template<typename T>
        void preAllocate(size_t count) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto typeIndex = std::type_index(typeid(T));
            auto& typePool = m_typePools[typeIndex];
            
            for (size_t i = 0; i < count; ++i) {
                auto entity = std::make_shared<T>();
                cleanupEntity(entity); // Ensure clean state
                typePool.push_back(entity);
                m_stats.totalCreations++;
                m_stats.availableEntities++;
            }
        }

        /**
         * @brief Clear all entities from a specific type pool
         * @tparam T Entity type
         */
        template<typename T>
        void clearPool() {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto typeIndex = std::type_index(typeid(T));
            auto it = m_typePools.find(typeIndex);
            if (it != m_typePools.end()) {
                size_t count = it->second.size();
                it->second.clear();
                m_stats.availableEntities -= count;
                m_stats.totalDestructions += count;
            }
        }

        /**
         * @brief Clear all pools
         */
        void clearAllPools();

        /**
         * @brief Force return of all active entities to their pools
         * @warning This should only be used during cleanup or testing
         */
        void forceReturnAllActive();

        /**
         * @brief Shrink pools by removing unused entities
         * @param forceImmediate If true, ignore shrink interval timing
         */
        void shrinkPools(bool forceImmediate = false);

        /**
         * @brief Get current pool statistics
         * @return Pool statistics structure
         */
        PoolStats getStats() const;

        /**
         * @brief Get pool configuration
         * @return Current pool configuration
         */
        const PoolConfig& getConfig() const { return m_config; }

        /**
         * @brief Update pool configuration
         * @param config New configuration
         */
        void updateConfig(const PoolConfig& config);

        /**
         * @brief Get the number of available entities for a specific type
         * @tparam T Entity type
         * @return Number of available entities
         */
        template<typename T>
        size_t getAvailableCount() const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto typeIndex = std::type_index(typeid(T));
            auto it = m_typePools.find(typeIndex);
            return (it != m_typePools.end()) ? it->second.size() : 0;
        }

        /**
         * @brief Get the number of active entities for a specific type
         * @tparam T Entity type
         * @return Number of active entities
         */
        template<typename T>
        size_t getActiveCount() const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            size_t count = 0;
            for (const auto& entity : m_activeEntities) {
                if (std::dynamic_pointer_cast<T>(entity)) {
                    count++;
                }
            }
            return count;
        }

        /**
         * @brief Check if pool is healthy (good hit ratio, reasonable sizes)
         * @return True if pool metrics indicate good performance
         */
        bool isHealthy() const;

    private:
        PoolConfig m_config;
        mutable std::mutex m_mutex;
        
        // Type-specific entity pools
        std::unordered_map<std::type_index, std::vector<std::shared_ptr<Entity>>> m_typePools;
        
        // Active (borrowed) entities tracking
        std::unordered_set<std::shared_ptr<Entity>> m_activeEntities;
        
        // Statistics
        mutable PoolStats m_stats;
        
        /**
         * @brief Reset an entity to a clean state for reuse
         * @param entity Entity to reset
         */
        void resetEntity(std::shared_ptr<Entity> entity);
        
        /**
         * @brief Clean up an entity before returning to pool
         * @param entity Entity to clean up
         */
        void cleanupEntity(std::shared_ptr<Entity> entity);
        
        /**
         * @brief Update hit ratio statistics
         */
        void updateHitRatio();
        
        /**
         * @brief Check if pools should be shrunk
         * @return True if shrinking is recommended
         */
        bool shouldShrink() const;
    };

    /**
     * @brief Singleton Entity Pool Manager
     * 
     * Provides global access to entity pooling functionality
     */
    class EntityPoolManager {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the EntityPoolManager instance
         */
        static EntityPoolManager& getInstance();

        /**
         * @brief Get the default entity pool
         * @return Reference to the default entity pool
         */
        EntityPool& getDefaultPool() { return m_defaultPool; }

        /**
         * @brief Get a named entity pool
         * @param poolName Name of the pool
         * @return Reference to the named entity pool
         */
        EntityPool& getPool(const std::string& poolName);

        /**
         * @brief Create a new named pool with specific configuration
         * @param poolName Name of the new pool
         * @param config Pool configuration
         * @return Reference to the created pool
         */
        EntityPool& createPool(const std::string& poolName, const EntityPool::PoolConfig& config);

        /**
         * @brief Remove a named pool
         * @param poolName Name of the pool to remove
         * @return True if pool was removed successfully
         */
        bool removePool(const std::string& poolName);

        /**
         * @brief Get all pool names
         * @return Vector of pool names
         */
        std::vector<std::string> getPoolNames() const;

        /**
         * @brief Get aggregate statistics from all pools
         * @return Combined statistics
         */
        EntityPool::PoolStats getAggregateStats() const;

        /**
         * @brief Update all pools (shrinking, cleanup, etc.)
         */
        void updateAllPools();

        /**
         * @brief Clear all pools
         */
        void clearAllPools();

    private:
        EntityPoolManager();
        ~EntityPoolManager() = default;

        // Non-copyable and non-movable
        EntityPoolManager(const EntityPoolManager&) = delete;
        EntityPoolManager& operator=(const EntityPoolManager&) = delete;
        EntityPoolManager(EntityPoolManager&&) = delete;
        EntityPoolManager& operator=(EntityPoolManager&&) = delete;

        EntityPool m_defaultPool;
        std::unordered_map<std::string, std::unique_ptr<EntityPool>> m_namedPools;
        mutable std::mutex m_mutex;
    };

    /**
     * @brief Convenience function to get the default entity pool
     * @return Reference to the default entity pool
     */
    inline EntityPool& getEntityPool() {
        return EntityPoolManager::getInstance().getDefaultPool();
    }

    /**
     * @brief RAII wrapper for automatic entity return to pool
     * @tparam T Entity type
     */
    template<typename T>
    class PooledEntity {
    public:
        /**
         * @brief Constructor with entity and pool
         * @param entity The pooled entity
         * @param pool The pool to return entity to
         */
        PooledEntity(std::shared_ptr<T> entity, EntityPool* pool)
            : m_entity(entity), m_pool(pool) {}

        /**
         * @brief Destructor - automatically returns entity to pool
         */
        ~PooledEntity() {
            if (m_entity && m_pool) {
                m_pool->returnEntity(m_entity);
            }
        }

        // Non-copyable but movable
        PooledEntity(const PooledEntity&) = delete;
        PooledEntity& operator=(const PooledEntity&) = delete;

        PooledEntity(PooledEntity&& other) noexcept
            : m_entity(std::move(other.m_entity)), m_pool(other.m_pool) {
            other.m_pool = nullptr;
        }

        PooledEntity& operator=(PooledEntity&& other) noexcept {
            if (this != &other) {
                if (m_entity && m_pool) {
                    m_pool->returnEntity(m_entity);
                }
                m_entity = std::move(other.m_entity);
                m_pool = other.m_pool;
                other.m_pool = nullptr;
            }
            return *this;
        }

        /**
         * @brief Access the underlying entity
         * @return Pointer to the entity
         */
        T* operator->() { return m_entity.get(); }
        const T* operator->() const { return m_entity.get(); }

        /**
         * @brief Dereference the entity
         * @return Reference to the entity
         */
        T& operator*() { return *m_entity; }
        const T& operator*() const { return *m_entity; }

        /**
         * @brief Get the shared pointer to the entity
         * @return Shared pointer to the entity
         */
        std::shared_ptr<T> get() { return m_entity; }
        const std::shared_ptr<T> get() const { return m_entity; }

        /**
         * @brief Release the entity from RAII management
         * @return Shared pointer to the entity (caller responsible for returning)
         */
        std::shared_ptr<T> release() {
            m_pool = nullptr;
            return std::move(m_entity);
        }

        /**
         * @brief Check if the wrapper contains a valid entity
         * @return True if entity is valid
         */
        explicit operator bool() const { return m_entity != nullptr; }

    private:
        std::shared_ptr<T> m_entity;
        EntityPool* m_pool;
    };

} // namespace IKore
