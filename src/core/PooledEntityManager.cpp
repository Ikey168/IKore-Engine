#include "PooledEntityManager.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>

namespace IKore {

    PooledEntityManager::~PooledEntityManager() {
        cleanupAll();
    }

    PooledEntityManager& PooledEntityManager::getInstance() {
        static PooledEntityManager instance;
        return instance;
    }

    bool PooledEntityManager::destroyEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return false;
        }
        
        return destroyEntity(entity->getID());
    }

    bool PooledEntityManager::destroyEntity(EntityID id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entities.find(id);
        if (it != m_entities.end()) {
            EntityInfo& info = it->second;
            
            // Clean up entity before returning to pool
            if (info.entity) {
                info.entity->cleanup();
            }
            
            // Return to pool
            if (info.pool && info.entity) {
                info.pool->returnEntity(info.entity);
            }
            
            m_entities.erase(it);
            LOG_DEBUG("Destroyed pooled entity with ID: " + std::to_string(id));
            return true;
        }
        
        return false;
    }

    std::shared_ptr<Entity> PooledEntityManager::getEntity(EntityID id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entities.find(id);
        if (it != m_entities.end() && !it->second.markedForDeletion) {
            return it->second.entity;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Entity>> PooledEntityManager::getAllEntities() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<std::shared_ptr<Entity>> entities;
        entities.reserve(m_entities.size());
        
        for (const auto& pair : m_entities) {
            if (pair.second.entity && !pair.second.markedForDeletion) {
                entities.push_back(pair.second.entity);
            }
        }
        
        return entities;
    }

    void PooledEntityManager::forEach(std::function<void(std::shared_ptr<Entity>)> callback) const {
        if (!callback) {
            LOG_WARNING("PooledEntityManager::forEach called with null callback");
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_entities) {
            if (pair.second.entity && !pair.second.markedForDeletion) {
                callback(pair.second.entity);
            }
        }
    }

    size_t PooledEntityManager::getEntityCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t count = 0;
        for (const auto& pair : m_entities) {
            if (!pair.second.markedForDeletion) {
                count++;
            }
        }
        return count;
    }

    bool PooledEntityManager::hasEntity(EntityID id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entities.find(id);
        return (it != m_entities.end() && !it->second.markedForDeletion);
    }

    void PooledEntityManager::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t entityCount = 0;
        for (auto& pair : m_entities) {
            EntityInfo& info = pair.second;
            
            // Clean up entity
            if (info.entity) {
                info.entity->cleanup();
                entityCount++;
            }
            
            // Return to pool
            if (info.pool && info.entity) {
                info.pool->returnEntity(info.entity);
            }
        }
        
        m_entities.clear();
        m_deletionQueue.clear();
        
        LOG_INFO("PooledEntityManager cleared " + std::to_string(entityCount) + " entities");
    }

    void PooledEntityManager::updateAll(float deltaTime) {
        // Auto garbage collection check
        autoGarbageCollect();
        
        std::vector<std::shared_ptr<Entity>> entitiesToUpdate;
        
        // Collect entities to update (with lock)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            entitiesToUpdate.reserve(m_entities.size());
            
            for (const auto& pair : m_entities) {
                if (pair.second.entity && !pair.second.markedForDeletion && 
                    pair.second.entity->isValid()) {
                    entitiesToUpdate.push_back(pair.second.entity);
                }
            }
        }
        
        // Update entities outside the lock to avoid deadlock
        for (auto& entity : entitiesToUpdate) {
            try {
                entity->update(deltaTime);
            } catch (const std::exception& e) {
                LOG_ERROR("Exception during pooled entity update for ID " + 
                         std::to_string(entity->getID()) + ": " + e.what());
            }
        }
    }

    void PooledEntityManager::initializeAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t initializedCount = 0;
        for (auto& pair : m_entities) {
            if (pair.second.entity && !pair.second.markedForDeletion && 
                pair.second.entity->isValid()) {
                try {
                    pair.second.entity->initialize();
                    initializedCount++;
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception during pooled entity initialization for ID " + 
                             std::to_string(pair.second.entity->getID()) + ": " + e.what());
                }
            }
        }
        
        LOG_INFO("Initialized " + std::to_string(initializedCount) + " pooled entities");
    }

    void PooledEntityManager::cleanupAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t cleanedCount = 0;
        for (auto& pair : m_entities) {
            if (pair.second.entity) {
                try {
                    pair.second.entity->cleanup();
                    cleanedCount++;
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception during pooled entity cleanup for ID " + 
                             std::to_string(pair.second.entity->getID()) + ": " + e.what());
                }
                
                // Return to pool
                if (pair.second.pool) {
                    pair.second.pool->returnEntity(pair.second.entity);
                }
            }
        }
        
        m_entities.clear();
        m_deletionQueue.clear();
        
        LOG_INFO("Cleaned up " + std::to_string(cleanedCount) + " pooled entities");
    }

    PooledEntityManager::PooledEntityStats PooledEntityManager::getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        PooledEntityStats stats;
        stats.totalEntities = getEntityCount();
        stats.aggregatePoolStats = EntityPoolManager::getInstance().getAggregateStats();
        
        // Count entities by pool
        auto& defaultPool = EntityPoolManager::getInstance().getDefaultPool();
        
        for (const auto& pair : m_entities) {
            if (!pair.second.markedForDeletion && pair.second.pool) {
                if (pair.second.pool == &defaultPool) {
                    stats.entitiesFromDefaultPool++;
                } else {
                    stats.entitiesFromNamedPools++;
                }
            }
        }
        
        // Get pool names and count entities per pool
        auto poolNames = EntityPoolManager::getInstance().getPoolNames();
        for (const auto& name : poolNames) {
            stats.entitiesByPool[name] = 0;
        }
        
        // This is a simplified count - in a real implementation, you'd track pool names
        stats.entitiesByPool["default"] = stats.entitiesFromDefaultPool;
        stats.entitiesByPool["named_pools"] = stats.entitiesFromNamedPools;
        
        return stats;
    }

    size_t PooledEntityManager::garbageCollect() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t collected = 0;
        
        // Process deletion queue
        for (EntityID id : m_deletionQueue) {
            auto it = m_entities.find(id);
            if (it != m_entities.end()) {
                EntityInfo& info = it->second;
                
                // Clean up entity
                if (info.entity) {
                    info.entity->cleanup();
                }
                
                // Return to pool
                if (info.pool && info.entity) {
                    info.pool->returnEntity(info.entity);
                }
                
                m_entities.erase(it);
                collected++;
            }
        }
        
        m_deletionQueue.clear();
        m_lastGC = std::chrono::steady_clock::now();
        
        if (collected > 0) {
            LOG_DEBUG("Garbage collected " + std::to_string(collected) + " pooled entities");
        }
        
        return collected;
    }

    void PooledEntityManager::markForDeletion(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }
        
        markForDeletion(entity->getID());
    }

    void PooledEntityManager::markForDeletion(EntityID id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entities.find(id);
        if (it != m_entities.end() && !it->second.markedForDeletion) {
            it->second.markedForDeletion = true;
            m_deletionQueue.push_back(id);
            LOG_DEBUG("Marked pooled entity for deletion: " + std::to_string(id));
        }
    }

    void PooledEntityManager::setAutoGCInterval(uint32_t intervalMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_autoGCInterval = intervalMs;
        LOG_DEBUG("Set auto GC interval to " + std::to_string(intervalMs) + " ms");
    }

    void PooledEntityManager::setAutoGCEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_autoGCEnabled = enabled;
        if (enabled) {
            m_lastGC = std::chrono::steady_clock::now();
        }
        LOG_DEBUG("Auto GC " + std::string(enabled ? "enabled" : "disabled"));
    }

    void PooledEntityManager::addEntity(std::shared_ptr<Entity> entity, EntityPool* pool) {
        if (!entity) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        EntityInfo info;
        info.entity = entity;
        info.pool = pool;
        info.markedForDeletion = false;
        
        m_entities[entity->getID()] = info;
        LOG_DEBUG("Added pooled entity to tracking: " + std::to_string(entity->getID()));
    }

    bool PooledEntityManager::removeEntity(EntityID id) {
        auto it = m_entities.find(id);
        if (it != m_entities.end()) {
            m_entities.erase(it);
            return true;
        }
        return false;
    }

    bool PooledEntityManager::shouldRunAutoGC() const {
        if (!m_autoGCEnabled || m_autoGCInterval == 0 || m_deletionQueue.empty()) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastGC);
        
        return elapsed.count() >= m_autoGCInterval;
    }

    void PooledEntityManager::autoGarbageCollect() {
        if (shouldRunAutoGC()) {
            garbageCollect();
        }
    }

} // namespace IKore
