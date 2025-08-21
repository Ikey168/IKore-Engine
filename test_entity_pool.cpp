#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <thread>
#include <random>

// Include the entity pooling system
#include "src/core/EntityPool.h"
#include "src/core/Entity.h"
#include "src/core/Logger.h"

using namespace IKore;

// Test entity classes
class TestEntity : public Entity {
public:
    TestEntity() : value(0), initialized(false) {}
    
    void setValue(int v) { value = v; }
    int getValue() const { return value; }
    
    void initialize() override {
        initialized = true;
        Entity::initialize();
    }
    
    void cleanup() override {
        initialized = false;
        value = 0;
        Entity::cleanup();
    }
    
    bool isInitialized() const { return initialized; }

private:
    int value;
    bool initialized;
};

class GameEntity : public Entity {
public:
    GameEntity() : health(100), level(1) {}
    
    void setHealth(int h) { health = h; }
    int getHealth() const { return health; }
    
    void setLevel(int l) { level = l; }
    int getLevel() const { return level; }
    
    void cleanup() override {
        health = 100;
        level = 1;
        Entity::cleanup();
    }

private:
    int health;
    int level;
};

// Performance test function
void performanceTest() {
    std::cout << "\n=== Entity Pool Performance Test ===" << std::endl;
    
    // Configure pool for performance testing
    EntityPool::PoolConfig config;
    config.initialSize = 100;
    config.maxSize = 1000;
    config.growthSize = 50;
    config.preAllocate = true;
    
    EntityPool pool(config);
    
    // Pre-allocate entities
    pool.preAllocate<TestEntity>(config.initialSize);
    
    const int NUM_OPERATIONS = 10000;
    std::vector<std::shared_ptr<TestEntity>> entities;
    entities.reserve(NUM_OPERATIONS);
    
    // Test entity borrowing performance
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto entity = pool.borrowEntity<TestEntity>();
        entity->setValue(i);
        entities.push_back(entity);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // Test entity returning performance
    for (auto& entity : entities) {
        pool.returnEntity(entity);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto borrowTime = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto returnTime = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Borrow time: " << borrowTime.count() << " microseconds" << std::endl;
    std::cout << "Return time: " << returnTime.count() << " microseconds" << std::endl;
    std::cout << "Total time: " << totalTime.count() << " microseconds" << std::endl;
    std::cout << "Operations per second: " << (NUM_OPERATIONS * 2 * 1000000.0 / totalTime.count()) << std::endl;
    
    // Print pool statistics
    auto stats = pool.getStats();
    std::cout << "\nPool Statistics:" << std::endl;
    std::cout << "- Total entities: " << stats.totalEntities << std::endl;
    std::cout << "- Active entities: " << stats.activeEntities << std::endl;
    std::cout << "- Available entities: " << stats.availableEntities << std::endl;
    std::cout << "- Pool hits: " << stats.poolHits << std::endl;
    std::cout << "- Pool misses: " << stats.poolMisses << std::endl;
    std::cout << "- Hit ratio: " << (stats.hitRatio * 100.0f) << "%" << std::endl;
    std::cout << "- Total creations: " << stats.totalCreations << std::endl;
    std::cout << "- Pool healthy: " << (pool.isHealthy() ? "Yes" : "No") << std::endl;
}

// Memory efficiency test
void memoryEfficiencyTest() {
    std::cout << "\n=== Memory Efficiency Test ===" << std::endl;
    
    EntityPool::PoolConfig config;
    config.initialSize = 50;
    config.maxSize = 200;
    config.enableShrinking = true;
    config.shrinkThreshold = 0.5f;
    
    EntityPool pool(config);
    
    // Create and use many entities
    std::vector<std::shared_ptr<GameEntity>> entities;
    
    std::cout << "Creating 150 entities..." << std::endl;
    for (int i = 0; i < 150; ++i) {
        auto entity = pool.borrowEntity<GameEntity>();
        entity->setHealth(100 + i);
        entity->setLevel(i % 10 + 1);
        entities.push_back(entity);
    }
    
    auto stats1 = pool.getStats();
    std::cout << "After creation - Total: " << stats1.totalEntities 
              << ", Active: " << stats1.activeEntities 
              << ", Available: " << stats1.availableEntities << std::endl;
    
    // Return most entities
    std::cout << "Returning 120 entities..." << std::endl;
    for (int i = 0; i < 120; ++i) {
        pool.returnEntity(entities[i]);
    }
    entities.erase(entities.begin(), entities.begin() + 120);
    
    auto stats2 = pool.getStats();
    std::cout << "After returns - Total: " << stats2.totalEntities 
              << ", Active: " << stats2.activeEntities 
              << ", Available: " << stats2.availableEntities << std::endl;
    
    // Force shrink to test memory cleanup
    std::cout << "Forcing pool shrink..." << std::endl;
    pool.shrinkPools(true);
    
    auto stats3 = pool.getStats();
    std::cout << "After shrink - Total: " << stats3.totalEntities 
              << ", Active: " << stats3.activeEntities 
              << ", Available: " << stats3.availableEntities << std::endl;
    
    // Clean up remaining entities
    for (auto& entity : entities) {
        pool.returnEntity(entity);
    }
}

// Multi-threaded stress test
void multiThreadedTest() {
    std::cout << "\n=== Multi-threaded Stress Test ===" << std::endl;
    
    EntityPool::PoolConfig config;
    config.initialSize = 100;
    config.maxSize = 0; // Unlimited
    config.growthSize = 25;
    
    EntityPool pool(config);
    
    const int NUM_THREADS = 4;
    const int OPERATIONS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> totalOperations{0};
    
    auto workerFunction = [&pool, &totalOperations, OPERATIONS_PER_THREAD](int threadId) {
        std::mt19937 rng(threadId);
        std::uniform_int_distribution<int> dist(1, 100);
        
        for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            // Borrow entity
            auto entity = pool.borrowEntity<TestEntity>();
            entity->setValue(dist(rng));
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            
            // Return entity
            pool.returnEntity(entity);
            
            totalOperations.fetch_add(1);
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Start threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(workerFunction, i);
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed " << totalOperations.load() << " operations in " 
              << duration.count() << " milliseconds" << std::endl;
    std::cout << "Operations per second: " 
              << (totalOperations.load() * 1000.0 / duration.count()) << std::endl;
    
    auto stats = pool.getStats();
    std::cout << "Final pool stats - Hit ratio: " << (stats.hitRatio * 100.0f) 
              << "%, Total entities: " << stats.totalEntities << std::endl;
}

// RAII wrapper test
void raiiWrapperTest() {
    std::cout << "\n=== RAII Wrapper Test ===" << std::endl;
    
    EntityPool pool;
    
    {
        // Test automatic return with PooledEntity wrapper
        std::cout << "Creating PooledEntity wrapper..." << std::endl;
        auto pooledEntity = PooledEntity<TestEntity>(
            pool.borrowEntity<TestEntity>(), &pool);
        
        pooledEntity->setValue(42);
        std::cout << "Entity value: " << pooledEntity->getValue() << std::endl;
        
        auto stats1 = pool.getStats();
        std::cout << "Active entities: " << stats1.activeEntities << std::endl;
        
        // pooledEntity will automatically return entity when going out of scope
    }
    
    std::cout << "PooledEntity wrapper destroyed" << std::endl;
    auto stats2 = pool.getStats();
    std::cout << "Active entities after destruction: " << stats2.activeEntities << std::endl;
}

// Entity Pool Manager test
void poolManagerTest() {
    std::cout << "\n=== Entity Pool Manager Test ===" << std::endl;
    
    auto& manager = EntityPoolManager::getInstance();
    
    // Test default pool
    auto& defaultPool = manager.getDefaultPool();
    auto entity1 = defaultPool.borrowEntity<TestEntity>();
    entity1->setValue(100);
    
    // Create named pools with different configurations
    EntityPool::PoolConfig fastConfig;
    fastConfig.initialSize = 200;
    fastConfig.maxSize = 500;
    
    EntityPool::PoolConfig slowConfig;
    slowConfig.initialSize = 10;
    slowConfig.maxSize = 50;
    
    auto& fastPool = manager.createPool("fast", fastConfig);
    auto& slowPool = manager.createPool("slow", slowConfig);
    
    // Test entities from different pools
    auto entity2 = fastPool.borrowEntity<GameEntity>();
    auto entity3 = slowPool.borrowEntity<TestEntity>();
    
    entity2->setHealth(200);
    entity3->setValue(300);
    
    std::cout << "Entity 1 value: " << entity1->getValue() << std::endl;
    std::cout << "Entity 2 health: " << entity2->getHealth() << std::endl;
    std::cout << "Entity 3 value: " << entity3->getValue() << std::endl;
    
    // Return entities
    defaultPool.returnEntity(entity1);
    fastPool.returnEntity(entity2);
    slowPool.returnEntity(entity3);
    
    // Get pool names
    auto poolNames = manager.getPoolNames();
    std::cout << "Available pools: ";
    for (const auto& name : poolNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // Get aggregate statistics
    auto aggregateStats = manager.getAggregateStats();
    std::cout << "Aggregate stats - Total borrows: " << aggregateStats.totalBorrows 
              << ", Hit ratio: " << (aggregateStats.hitRatio * 100.0f) << "%" << std::endl;
}

int main() {
    // Initialize logger
    Logger::getInstance().initialize();
    
    std::cout << "=== Entity Pool System Test ===" << std::endl;
    
    try {
        // Run all tests
        performanceTest();
        memoryEfficiencyTest();
        multiThreadedTest();
        raiiWrapperTest();
        poolManagerTest();
        
        std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "\nTest failed with exception: " << e.what() << std::endl;
        Logger::getInstance().shutdown();
        return 1;
    }
    
    Logger::getInstance().shutdown();
    return 0;
}
