#include "ECSBenchmark.h"
#include "Entity.h"
#include "components/TransformComponent.h"
#include "components/VelocityComponent.h"
#include "components/RenderableComponent.h"
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>
#include <iomanip>
#include <sstream>

namespace IKore {

    ECSBenchmark::ECSBenchmark() 
        : m_memoryProfilingEnabled(true)
        , m_realTimeMonitoring(false) {
        resetBenchmark();
    }

    ECSBenchmark::~ECSBenchmark() {
        if (m_realTimeMonitoring) {
            stopRealTimeMonitoring();
        }
    }

    PerformanceMetrics ECSBenchmark::runComprehensiveBenchmark() {
        std::cout << "=== Running Comprehensive ECS Benchmark ===" << std::endl;
        
        std::vector<PerformanceMetrics> allMetrics;
        
        // Test with default entity counts
        for (size_t entityCount : m_config.entityCounts) {
            std::cout << "\n--- Testing with " << entityCount << " entities ---" << std::endl;
            
            auto entityMetrics = benchmarkEntityOperations(entityCount);
            auto componentMetrics = benchmarkComponentOperations(entityCount);
            auto lookupMetrics = benchmarkLookupPerformance(entityCount, m_config.lookupIterations);
            auto iterationMetrics = benchmarkIterationPerformance(entityCount);
            auto memoryMetrics = benchmarkMemoryUsage(entityCount);
            
            // Combine metrics
            PerformanceMetrics combined;
            combined.entityCount = entityCount;
            combined.entityCreationTime = entityMetrics.entityCreationTime;
            combined.componentAddTime = componentMetrics.componentAddTime;
            combined.componentRemovalTime = componentMetrics.componentRemovalTime;
            combined.componentLookupTime = lookupMetrics.componentLookupTime;
            combined.entityIterationTime = iterationMetrics.entityIterationTime;
            combined.entityMemoryUsage = memoryMetrics.entityMemoryUsage;
            combined.componentMemoryUsage = memoryMetrics.componentMemoryUsage;
            combined.totalMemoryUsage = memoryMetrics.totalMemoryUsage;
            combined.componentCount = componentMetrics.componentCount;
            
            // Calculate performance ratios
            combined.entitiesPerSecond = 1000000.0 / combined.entityCreationTime; // Convert from microseconds
            combined.componentsPerSecond = 1000000.0 / combined.componentAddTime;
            combined.lookupsPerSecond = 1000000.0 / combined.componentLookupTime;
            
            allMetrics.push_back(combined);
            logMetrics(combined, "EntityCount_" + std::to_string(entityCount));
        }
        
        // Generate comprehensive report
        generateDetailedReport(allMetrics);
        
        // Return average metrics
        return calculateAverageMetrics(allMetrics);
    }

    PerformanceMetrics ECSBenchmark::runBenchmarkWithConfig(const BenchmarkConfig& config) {
        auto oldConfig = m_config;
        m_config = config;
        
        auto result = runComprehensiveBenchmark();
        
        m_config = oldConfig;
        return result;
    }

    std::vector<PerformanceMetrics> ECSBenchmark::runScalabilityTest() {
        std::cout << "=== Running ECS Scalability Test ===" << std::endl;
        
        std::vector<PerformanceMetrics> scalabilityResults;
        
        // Test with increasing entity counts to measure scalability
        std::vector<size_t> scalabilityEntityCounts = {
            100, 250, 500, 750, 1000, 2000, 3000, 4000, 5000, 7500, 10000
        };
        
        for (size_t entityCount : scalabilityEntityCounts) {
            std::cout << "\n--- Scalability test with " << entityCount << " entities ---" << std::endl;
            
            auto metrics = benchmarkEntityOperations(entityCount);
            auto componentMetrics = benchmarkComponentOperations(entityCount);
            auto iterationMetrics = benchmarkIterationPerformance(entityCount);
            
            // Combine key scalability metrics
            PerformanceMetrics scalabilityMetrics;
            scalabilityMetrics.entityCount = entityCount;
            scalabilityMetrics.entityCreationTime = metrics.entityCreationTime;
            scalabilityMetrics.componentAddTime = componentMetrics.componentAddTime;
            scalabilityMetrics.componentLookupTime = componentMetrics.componentLookupTime;
            scalabilityMetrics.entityIterationTime = iterationMetrics.entityIterationTime;
            
            // Calculate throughput metrics
            scalabilityMetrics.entitiesPerSecond = 1000000.0 / scalabilityMetrics.entityCreationTime;
            scalabilityMetrics.componentsPerSecond = 1000000.0 / scalabilityMetrics.componentAddTime;
            scalabilityMetrics.lookupsPerSecond = 1000000.0 / scalabilityMetrics.componentLookupTime;
            
            scalabilityResults.push_back(scalabilityMetrics);
            
            std::cout << "  Entity creation: " << std::fixed << std::setprecision(2) 
                      << scalabilityMetrics.entityCreationTime << " μs/entity" << std::endl;
            std::cout << "  Component ops: " << std::fixed << std::setprecision(2) 
                      << scalabilityMetrics.componentAddTime << " μs/component" << std::endl;
            std::cout << "  Iteration time: " << std::fixed << std::setprecision(2) 
                      << scalabilityMetrics.entityIterationTime << " μs total" << std::endl;
        }
        
        // Export scalability results
        exportToCsv(scalabilityResults, "ecs_scalability_results.csv");
        
        return scalabilityResults;
    }

    PerformanceMetrics ECSBenchmark::benchmarkEntityOperations(size_t entityCount) {
        PerformanceMetrics metrics;
        Timer timer;
        
        // Benchmark entity creation
        std::vector<std::shared_ptr<Entity>> entities;
        entities.reserve(entityCount);
        
        timer.reset();
        for (size_t i = 0; i < entityCount; ++i) {
            entities.push_back(std::make_shared<Entity>());
        }
        metrics.entityCreationTime = timer.elapsedMicroseconds() / entityCount;
        
        metrics.entityCount = entityCount;
        metrics.activeEntities = entityCount;
        metrics.inactiveEntities = 0;
        
        // Track memory if enabled
        if (m_memoryProfilingEnabled) {
            metrics.entityMemoryUsage = entityCount * sizeof(Entity);
            trackMemoryAllocation(metrics.entityMemoryUsage, "Entity");
        }
        
        // Cleanup
        entities.clear();
        
        return metrics;
    }

    PerformanceMetrics ECSBenchmark::benchmarkComponentOperations(size_t entityCount) {
        PerformanceMetrics metrics;
        Timer timer;
        
        // Create test entities
        auto entities = createTestEntities(entityCount);
        
        // Benchmark component addition
        timer.reset();
        for (auto& entity : entities) {
            entity->addComponent<TransformComponent>();
            entity->addComponent<VelocityComponent>();
            if (m_config.componentTypesPerEntity >= 3) {
                entity->addComponent<RenderableComponent>();
            }
        }
        metrics.componentAddTime = timer.elapsedMicroseconds() / (entityCount * m_config.componentTypesPerEntity);
        
        // Benchmark component lookup
        timer.reset();
        for (size_t i = 0; i < m_config.lookupIterations; ++i) {
            for (auto& entity : entities) {
                auto transform = entity->getComponent<TransformComponent>();
                auto velocity = entity->getComponent<VelocityComponent>();
                (void)transform; (void)velocity; // Suppress warnings
            }
        }
        metrics.componentLookupTime = timer.elapsedMicroseconds() / (entityCount * m_config.lookupIterations * 2);
        
        // Benchmark component removal
        timer.reset();
        for (auto& entity : entities) {
            entity->removeComponent<VelocityComponent>();
        }
        metrics.componentRemovalTime = timer.elapsedMicroseconds() / entityCount;
        
        metrics.entityCount = entityCount;
        metrics.componentCount = entityCount * m_config.componentTypesPerEntity;
        
        // Track component memory
        if (m_memoryProfilingEnabled) {
            size_t componentSize = sizeof(TransformComponent) + sizeof(VelocityComponent) + sizeof(RenderableComponent);
            metrics.componentMemoryUsage = entityCount * componentSize;
            trackMemoryAllocation(metrics.componentMemoryUsage, "Components");
        }
        
        cleanupTestEntities(entities);
        return metrics;
    }

    PerformanceMetrics ECSBenchmark::benchmarkLookupPerformance(size_t entityCount, size_t lookupCount) {
        PerformanceMetrics metrics;
        Timer timer;
        
        // Create entities with components
        auto entities = createTestEntities(entityCount);
        for (auto& entity : entities) {
            entity->addComponent<TransformComponent>();
            entity->addComponent<VelocityComponent>();
        }
        
        // Benchmark lookup performance
        timer.reset();
        for (size_t i = 0; i < lookupCount; ++i) {
            for (auto& entity : entities) {
                auto transform = entity->getComponent<TransformComponent>();
                bool hasVelocity = entity->hasComponent<VelocityComponent>();
                (void)transform; (void)hasVelocity; // Suppress warnings
            }
        }
        metrics.componentLookupTime = timer.elapsedMicroseconds() / (entityCount * lookupCount * 2);
        
        metrics.entityCount = entityCount;
        metrics.componentCount = entityCount * 2;
        metrics.lookupsPerSecond = 1000000.0 / metrics.componentLookupTime;
        
        cleanupTestEntities(entities);
        return metrics;
    }

    PerformanceMetrics ECSBenchmark::benchmarkIterationPerformance(size_t entityCount) {
        PerformanceMetrics metrics;
        Timer timer;
        
        // Create entities with components
        auto entities = createTestEntities(entityCount);
        for (auto& entity : entities) {
            entity->addComponent<TransformComponent>();
            entity->addComponent<VelocityComponent>();
        }
        
        // Benchmark entity iteration
        timer.reset();
        for (size_t iter = 0; iter < m_config.iterations; ++iter) {
            for (auto& entity : entities) {
                if (entity->hasComponent<TransformComponent>() && entity->hasComponent<VelocityComponent>()) {
                    auto transform = entity->getComponent<TransformComponent>();
                    auto velocity = entity->getComponent<VelocityComponent>();
                    
                    // Simulate simple update operation
                    transform->setPosition(transform->getPosition() + velocity->velocity * 0.016f); // 60 FPS
                }
            }
        }
        metrics.entityIterationTime = timer.elapsedMicroseconds() / m_config.iterations;
        
        metrics.entityCount = entityCount;
        metrics.systemUpdateTime = metrics.entityIterationTime;
        metrics.updatesPerSecond = 1000000.0 / metrics.entityIterationTime;
        
        cleanupTestEntities(entities);
        return metrics;
    }

    PerformanceMetrics ECSBenchmark::benchmarkMemoryUsage(size_t entityCount) {
        PerformanceMetrics metrics;
        
        // Reset memory tracking
        m_memoryTracker = MemoryTracker();
        
        // Create entities and track memory
        auto entities = createTestEntities(entityCount);
        metrics.entityMemoryUsage = entityCount * sizeof(Entity);
        trackMemoryAllocation(metrics.entityMemoryUsage, "Entity");
        
        // Add components and track memory
        for (auto& entity : entities) {
            entity->addComponent<TransformComponent>();
            entity->addComponent<VelocityComponent>();
            entity->addComponent<RenderableComponent>();
        }
        
        size_t componentSize = sizeof(TransformComponent) + sizeof(VelocityComponent) + sizeof(RenderableComponent);
        metrics.componentMemoryUsage = entityCount * componentSize;
        trackMemoryAllocation(metrics.componentMemoryUsage, "Components");
        
        metrics.totalMemoryUsage = getCurrentMemoryUsage();
        metrics.entityCount = entityCount;
        metrics.componentCount = entityCount * 3;
        
        cleanupTestEntities(entities);
        return metrics;
    }

    PerformanceMetrics ECSBenchmark::stressTestEntityCreation(size_t maxEntities) {
        std::cout << "=== Stress Testing Entity Creation (up to " << maxEntities << " entities) ===" << std::endl;
        
        PerformanceMetrics metrics;
        Timer timer;
        std::vector<std::shared_ptr<Entity>> entities;
        entities.reserve(maxEntities);
        
        timer.reset();
        for (size_t i = 0; i < maxEntities; ++i) {
            entities.push_back(std::make_shared<Entity>());
            
            // Log progress every 1000 entities
            if (i % 1000 == 0 && i > 0) {
                double currentTime = timer.elapsedMicroseconds();
                double avgTime = currentTime / i;
                std::cout << "  Created " << i << " entities, avg time: " 
                          << std::fixed << std::setprecision(2) << avgTime << " μs/entity" << std::endl;
            }
        }
        
        metrics.entityCreationTime = timer.elapsedMicroseconds() / maxEntities;
        metrics.entityCount = maxEntities;
        metrics.entitiesPerSecond = 1000000.0 / metrics.entityCreationTime;
        
        if (m_memoryProfilingEnabled) {
            metrics.entityMemoryUsage = maxEntities * sizeof(Entity);
            metrics.totalMemoryUsage = getCurrentMemoryUsage();
        }
        
        std::cout << "Stress test completed: " << std::fixed << std::setprecision(2) 
                  << metrics.entityCreationTime << " μs/entity" << std::endl;
        
        entities.clear();
        return metrics;
    }

    void ECSBenchmark::startRealTimeMonitoring() {
        m_realTimeMonitoring = true;
        std::cout << "Real-time ECS monitoring started" << std::endl;
    }

    void ECSBenchmark::stopRealTimeMonitoring() {
        m_realTimeMonitoring = false;
        std::cout << "Real-time ECS monitoring stopped" << std::endl;
    }

    void ECSBenchmark::generateReport(const std::vector<PerformanceMetrics>& results) {
        std::ostringstream report;
        
        report << "========================================\n";
        report << "        ECS PERFORMANCE REPORT          \n";
        report << "========================================\n\n";
        
        if (results.empty()) {
            report << "No benchmark results available.\n";
            std::cout << report.str();
            return;
        }
        
        auto avgMetrics = calculateAverageMetrics(results);
        
        report << "SUMMARY METRICS:\n";
        report << "----------------\n";
        report << std::fixed << std::setprecision(2);
        report << "Average entity creation time: " << avgMetrics.entityCreationTime << " μs/entity\n";
        report << "Average component add time: " << avgMetrics.componentAddTime << " μs/component\n";
        report << "Average component lookup time: " << avgMetrics.componentLookupTime << " μs/lookup\n";
        report << "Average iteration time: " << avgMetrics.entityIterationTime << " μs\n";
        report << "Average memory per entity: " << (avgMetrics.totalMemoryUsage / avgMetrics.entityCount) << " bytes\n\n";
        
        report << "PERFORMANCE VALIDATION:\n";
        report << "-----------------------\n";
        bool performanceOk = isPerformanceAcceptable(avgMetrics);
        report << "Overall performance: " << (performanceOk ? "✅ ACCEPTABLE" : "❌ NEEDS OPTIMIZATION") << "\n";
        
        auto warnings = getPerformanceWarnings(avgMetrics);
        if (!warnings.empty()) {
            report << "\nPerformance warnings:\n";
            for (const auto& warning : warnings) {
                report << "⚠️  " << warning << "\n";
            }
        }
        
        report << "\n";
        std::cout << report.str();
        
        if (m_config.outputToFile) {
            writeToFile(report.str(), m_config.outputFileName);
        }
    }

    void ECSBenchmark::generateDetailedReport(const std::vector<PerformanceMetrics>& results) {
        std::ostringstream report;
        
        report << "========================================\n";
        report << "     DETAILED ECS PERFORMANCE REPORT    \n";
        report << "========================================\n\n";
        
        report << std::fixed << std::setprecision(2);
        
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& metrics = results[i];
            
            report << "TEST " << (i + 1) << " - " << metrics.entityCount << " ENTITIES:\n";
            report << "----------------------------------------\n";
            report << "Entity creation time: " << metrics.entityCreationTime << " μs/entity\n";
            report << "Component add time: " << metrics.componentAddTime << " μs/component\n";
            report << "Component lookup time: " << metrics.componentLookupTime << " μs/lookup\n";
            report << "Entity iteration time: " << metrics.entityIterationTime << " μs total\n";
            report << "Memory usage: " << metrics.totalMemoryUsage << " bytes total\n";
            report << "Throughput: " << metrics.entitiesPerSecond << " entities/sec\n";
            
            bool testPassed = validatePerformance(metrics, m_config);
            report << "Performance: " << (testPassed ? "✅ PASS" : "❌ FAIL") << "\n\n";
        }
        
        std::cout << report.str();
        
        if (m_config.outputToFile) {
            writeToFile(report.str(), "ecs_detailed_report.txt");
        }
    }

    void ECSBenchmark::exportToCsv(const std::vector<PerformanceMetrics>& results, const std::string& filename) {
        std::ostringstream csv;
        
        // CSV header
        csv << "EntityCount,EntityCreationTime,ComponentAddTime,ComponentLookupTime,";
        csv << "IterationTime,MemoryUsage,EntitiesPerSecond,ComponentsPerSecond\n";
        
        // CSV data
        for (const auto& metrics : results) {
            csv << metrics.entityCount << ","
                << metrics.entityCreationTime << ","
                << metrics.componentAddTime << ","
                << metrics.componentLookupTime << ","
                << metrics.entityIterationTime << ","
                << metrics.totalMemoryUsage << ","
                << metrics.entitiesPerSecond << ","
                << metrics.componentsPerSecond << "\n";
        }
        
        writeToFile(csv.str(), filename);
        std::cout << "Results exported to " << filename << std::endl;
    }

    bool ECSBenchmark::isPerformanceAcceptable(const PerformanceMetrics& metrics) {
        return metrics.entityCreationTime < MAX_ENTITY_CREATION_TIME_US &&
               metrics.componentAddTime < MAX_COMPONENT_ADD_TIME_US &&
               metrics.componentLookupTime < MAX_LOOKUP_TIME_US &&
               (metrics.totalMemoryUsage / metrics.entityCount) < MAX_MEMORY_PER_ENTITY_BYTES;
    }

    std::vector<std::string> ECSBenchmark::getPerformanceWarnings(const PerformanceMetrics& metrics) {
        std::vector<std::string> warnings;
        
        if (metrics.entityCreationTime > MAX_ENTITY_CREATION_TIME_US) {
            warnings.push_back("Entity creation time exceeds threshold (" + 
                              std::to_string(metrics.entityCreationTime) + " μs > " + 
                              std::to_string(MAX_ENTITY_CREATION_TIME_US) + " μs)");
        }
        
        if (metrics.componentAddTime > MAX_COMPONENT_ADD_TIME_US) {
            warnings.push_back("Component addition time exceeds threshold (" + 
                              std::to_string(metrics.componentAddTime) + " μs > " + 
                              std::to_string(MAX_COMPONENT_ADD_TIME_US) + " μs)");
        }
        
        if (metrics.componentLookupTime > MAX_LOOKUP_TIME_US) {
            warnings.push_back("Component lookup time exceeds threshold (" + 
                              std::to_string(metrics.componentLookupTime) + " μs > " + 
                              std::to_string(MAX_LOOKUP_TIME_US) + " μs)");
        }
        
        size_t memoryPerEntity = metrics.totalMemoryUsage / std::max(metrics.entityCount, size_t(1));
        if (memoryPerEntity > MAX_MEMORY_PER_ENTITY_BYTES) {
            warnings.push_back("Memory usage per entity exceeds threshold (" + 
                              std::to_string(memoryPerEntity) + " bytes > " + 
                              std::to_string(MAX_MEMORY_PER_ENTITY_BYTES) + " bytes)");
        }
        
        return warnings;
    }

    // Helper methods implementation
    std::vector<std::shared_ptr<Entity>> ECSBenchmark::createTestEntities(size_t count) {
        std::vector<std::shared_ptr<Entity>> entities;
        entities.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            entities.push_back(std::make_shared<Entity>());
        }
        
        return entities;
    }

    void ECSBenchmark::cleanupTestEntities(std::vector<std::shared_ptr<Entity>>& entities) {
        entities.clear();
    }

    void ECSBenchmark::trackMemoryAllocation(size_t bytes, const std::string& source) {
        if (m_memoryProfilingEnabled) {
            m_memoryTracker.allocate(bytes, source);
        }
    }

    void ECSBenchmark::trackMemoryDeallocation(size_t bytes, const std::string& source) {
        if (m_memoryProfilingEnabled) {
            m_memoryTracker.deallocate(bytes, source);
        }
    }

    size_t ECSBenchmark::getCurrentMemoryUsage() const {
        return m_memoryTracker.currentUsage;
    }

    size_t ECSBenchmark::getPeakMemoryUsage() const {
        return m_memoryTracker.peakUsage;
    }

    void ECSBenchmark::logMetrics(const PerformanceMetrics& metrics, const std::string& testName) {
        if (m_config.enableDetailedLogging) {
            std::cout << "  " << testName << ": " << formatMetrics(metrics) << std::endl;
        }
    }

    std::string ECSBenchmark::formatMetrics(const PerformanceMetrics& metrics) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Entities: " << metrics.entityCount
           << ", Creation: " << metrics.entityCreationTime << "μs"
           << ", Lookup: " << metrics.componentLookupTime << "μs"
           << ", Memory: " << (metrics.totalMemoryUsage / 1024) << "KB";
        return ss.str();
    }

    void ECSBenchmark::writeToFile(const std::string& content, const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << content;
            file.close();
        }
    }

    PerformanceMetrics ECSBenchmark::calculateAverageMetrics(const std::vector<PerformanceMetrics>& metrics) {
        if (metrics.empty()) return PerformanceMetrics();
        
        PerformanceMetrics avg;
        
        for (const auto& m : metrics) {
            avg.entityCreationTime += m.entityCreationTime;
            avg.componentAddTime += m.componentAddTime;
            avg.componentLookupTime += m.componentLookupTime;
            avg.entityIterationTime += m.entityIterationTime;
            avg.entityMemoryUsage += m.entityMemoryUsage;
            avg.componentMemoryUsage += m.componentMemoryUsage;
            avg.totalMemoryUsage += m.totalMemoryUsage;
            avg.entityCount += m.entityCount;
            avg.componentCount += m.componentCount;
        }
        
        size_t count = metrics.size();
        avg.entityCreationTime /= count;
        avg.componentAddTime /= count;
        avg.componentLookupTime /= count;
        avg.entityIterationTime /= count;
        avg.entityMemoryUsage /= count;
        avg.componentMemoryUsage /= count;
        avg.totalMemoryUsage /= count;
        avg.entityCount /= count;
        avg.componentCount /= count;
        
        // Calculate derived metrics
        avg.entitiesPerSecond = 1000000.0 / avg.entityCreationTime;
        avg.componentsPerSecond = 1000000.0 / avg.componentAddTime;
        avg.lookupsPerSecond = 1000000.0 / avg.componentLookupTime;
        
        return avg;
    }

    bool ECSBenchmark::validatePerformance(const PerformanceMetrics& metrics, const BenchmarkConfig& config) {
        return isPerformanceAcceptable(metrics);
    }

    void ECSBenchmark::setConfig(const BenchmarkConfig& config) {
        m_config = config;
    }

    BenchmarkConfig ECSBenchmark::getConfig() const {
        return m_config;
    }

    void ECSBenchmark::resetBenchmark() {
        m_memoryTracker = MemoryTracker();
        m_currentMetrics = PerformanceMetrics();
        m_historicalMetrics.clear();
    }

} // namespace IKore
