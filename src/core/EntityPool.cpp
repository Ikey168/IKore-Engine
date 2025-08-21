#include "EntityPool.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>

namespace IKore {

    // ============================================================================
    // EntityPool Implementation
    // ============================================================================

    EntityPool::EntityPool(const PoolConfig& config) 
        : m_config(config) 
    {
        m_stats.lastShrink = std::chrono::steady_clock::now();
        
        LOG_INFO("EntityPool created with initial size: " + std::to_string(m_config.initialSize) + 
                ", max size: " + std::to_string(m_config.maxSize));
        
        // Pre-allocation will happen when specific types are requested
    }

    EntityPool::~EntityPool() {
        clearAllPools();
        forceReturnAllActive();
        
        LOG_INFO("EntityPool destroyed. Final stats - Total borrows: " + 
                std::to_string(m_stats.totalBorrows) + 
                ", Hit ratio: " + std::to_string(m_stats.hitRatio * 100.0f) + "%");
    }

    void EntityPool::clearAllPools() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t totalCleared = 0;
        for (auto& pair : m_typePools) {
            totalCleared += pair.second.size();
            pair.second.clear();
        }
        
        m_typePools.clear();
        m_stats.availableEntities = 0;
        m_stats.totalDestructions += totalCleared;
        
        LOG_INFO("Cleared all entity pools. Total entities destroyed: " + std::to_string(totalCleared));
    }

    void EntityPool::forceReturnAllActive() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t activeCount = m_activeEntities.size();
        if (activeCount > 0) {
            LOG_WARNING("Force returning " + std::to_string(activeCount) + " active entities to pool");
            
            // Clean up all active entities
            for (auto& entity : m_activeEntities) {
                cleanupEntity(entity);
            }
            
            m_activeEntities.clear();
            m_stats.activeEntities = 0;
        }
    }

    void EntityPool::shrinkPools(bool forceImmediate) {
        if (!m_config.enableShrinking && !forceImmediate) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        if (!forceImmediate) {
            auto timeSinceLastShrink = now - m_stats.lastShrink;
            if (timeSinceLastShrink < m_config.shrinkInterval) {
                return; // Too soon to shrink again
            }
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t totalShrunk = 0;
        for (auto& pair : m_typePools) {
            auto& pool = pair.second;
            if (pool.empty()) continue;
            
            // Calculate how many entities to remove
            size_t targetSize = static_cast<size_t>(pool.size() * (1.0f - m_config.shrinkThreshold));
            if (targetSize < pool.size()) {
                size_t toRemove = pool.size() - targetSize;
                
                // Remove entities from the end (they're likely older)
                pool.erase(pool.end() - toRemove, pool.end());
                totalShrunk += toRemove;
                m_stats.availableEntities -= toRemove;
                m_stats.totalDestructions += toRemove;
            }
        }
        
        m_stats.lastShrink = now;
        
        if (totalShrunk > 0) {
            LOG_DEBUG("Shrunk entity pools. Removed " + std::to_string(totalShrunk) + " unused entities");
        }
    }

    EntityPool::PoolStats EntityPool::getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update total entities count
        m_stats.totalEntities = m_stats.availableEntities + m_stats.activeEntities;
        
        return m_stats;
    }

    void EntityPool::updateConfig(const PoolConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        LOG_INFO("Updating EntityPool configuration");
        m_config = config;
        
        // If max size is reduced, shrink pools immediately
        if (config.maxSize > 0) {
            for (auto& pair : m_typePools) {
                auto& pool = pair.second;
                if (pool.size() > config.maxSize) {
                    size_t toRemove = pool.size() - config.maxSize;
                    pool.erase(pool.end() - toRemove, pool.end());
                    m_stats.availableEntities -= toRemove;
                    m_stats.totalDestructions += toRemove;
                }
            }
        }
    }

    bool EntityPool::isHealthy() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Consider pool healthy if:
        // 1. Hit ratio is above 70%
        // 2. Available entities are not excessive (less than 90% of total)
        // 3. We're not creating too many entities compared to reuse
        
        bool goodHitRatio = (m_stats.totalBorrows == 0) || (m_stats.hitRatio >= 0.7f);
        bool reasonableAvailability = (m_stats.totalEntities == 0) || 
                                    ((float)m_stats.availableEntities / m_stats.totalEntities < 0.9f);
        bool goodCreationRatio = (m_stats.totalBorrows == 0) || 
                               ((float)m_stats.totalCreations / m_stats.totalBorrows < 0.5f);
        
        return goodHitRatio && reasonableAvailability && goodCreationRatio;
    }

    void EntityPool::resetEntity(std::shared_ptr<Entity> entity) {
        if (!entity) return;
        
        // Reset entity to a clean state for reuse
        // This involves clearing components and resetting any state
        
        // Remove all components
        entity->removeAllComponents();
        
        // Reset any entity-specific state
        // Note: We don't reset the entity ID as it should remain unique
        
        LOG_DEBUG("Reset entity with ID: " + std::to_string(entity->getID()) + " for reuse");
    }

    void EntityPool::cleanupEntity(std::shared_ptr<Entity> entity) {
        if (!entity) return;
        
        // Clean up entity before storing in pool
        entity->cleanup();
        
        // Clear all components to free memory
        entity->removeAllComponents();
        
        LOG_DEBUG("Cleaned up entity with ID: " + std::to_string(entity->getID()) + " for pool storage");
    }

    void EntityPool::updateHitRatio() {
        if (m_stats.totalBorrows > 0) {
            m_stats.hitRatio = static_cast<float>(m_stats.poolHits) / m_stats.totalBorrows;
        } else {
            m_stats.hitRatio = 0.0f;
        }
    }

    bool EntityPool::shouldShrink() const {
        if (!m_config.enableShrinking) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastShrink = now - m_stats.lastShrink;
        
        if (timeSinceLastShrink < m_config.shrinkInterval) {
            return false;
        }
        
        // Check if we have too many available entities
        if (m_stats.totalEntities > 0) {
            float availableRatio = static_cast<float>(m_stats.availableEntities) / m_stats.totalEntities;
            return availableRatio > m_config.shrinkThreshold;
        }
        
        return false;
    }

    // ============================================================================
    // EntityPoolManager Implementation
    // ============================================================================

    EntityPoolManager::EntityPoolManager()
        : m_defaultPool(EntityPool::PoolConfig{}) 
    {
        LOG_INFO("EntityPoolManager initialized with default pool");
    }

    EntityPoolManager& EntityPoolManager::getInstance() {
        static EntityPoolManager instance;
        return instance;
    }

    EntityPool& EntityPoolManager::getPool(const std::string& poolName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_namedPools.find(poolName);
        if (it != m_namedPools.end()) {
            return *it->second;
        }
        
        // Create new pool with default configuration
        auto newPool = std::make_unique<EntityPool>();
        EntityPool* poolPtr = newPool.get();
        m_namedPools[poolName] = std::move(newPool);
        
        LOG_INFO("Created new entity pool: " + poolName);
        return *poolPtr;
    }

    EntityPool& EntityPoolManager::createPool(const std::string& poolName, const EntityPool::PoolConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto newPool = std::make_unique<EntityPool>(config);
        EntityPool* poolPtr = newPool.get();
        m_namedPools[poolName] = std::move(newPool);
        
        LOG_INFO("Created configured entity pool: " + poolName + 
                " (initial: " + std::to_string(config.initialSize) + 
                ", max: " + std::to_string(config.maxSize) + ")");
        
        return *poolPtr;
    }

    bool EntityPoolManager::removePool(const std::string& poolName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_namedPools.find(poolName);
        if (it != m_namedPools.end()) {
            // Force return all active entities before destroying pool
            it->second->forceReturnAllActive();
            m_namedPools.erase(it);
            LOG_INFO("Removed entity pool: " + poolName);
            return true;
        }
        
        LOG_WARNING("Attempted to remove non-existent pool: " + poolName);
        return false;
    }

    std::vector<std::string> EntityPoolManager::getPoolNames() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<std::string> names;
        names.reserve(m_namedPools.size() + 1);
        
        names.push_back("default");
        for (const auto& pair : m_namedPools) {
            names.push_back(pair.first);
        }
        
        return names;
    }

    EntityPool::PoolStats EntityPoolManager::getAggregateStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        EntityPool::PoolStats aggregate = m_defaultPool.getStats();
        
        for (const auto& pair : m_namedPools) {
            EntityPool::PoolStats poolStats = pair.second->getStats();
            
            aggregate.totalEntities += poolStats.totalEntities;
            aggregate.activeEntities += poolStats.activeEntities;
            aggregate.availableEntities += poolStats.availableEntities;
            aggregate.totalBorrows += poolStats.totalBorrows;
            aggregate.totalReturns += poolStats.totalReturns;
            aggregate.totalCreations += poolStats.totalCreations;
            aggregate.totalDestructions += poolStats.totalDestructions;
            aggregate.poolHits += poolStats.poolHits;
            aggregate.poolMisses += poolStats.poolMisses;
        }
        
        // Recalculate hit ratio for aggregate
        if (aggregate.totalBorrows > 0) {
            aggregate.hitRatio = static_cast<float>(aggregate.poolHits) / aggregate.totalBorrows;
        }
        
        return aggregate;
    }

    void EntityPoolManager::updateAllPools() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update default pool
        m_defaultPool.shrinkPools();
        
        // Update all named pools
        for (auto& pair : m_namedPools) {
            pair.second->shrinkPools();
        }
    }

    void EntityPoolManager::clearAllPools() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        LOG_INFO("Clearing all entity pools");
        
        m_defaultPool.clearAllPools();
        
        for (auto& pair : m_namedPools) {
            pair.second->clearAllPools();
        }
    }

} // namespace IKore
