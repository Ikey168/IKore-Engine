#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <iostream>
#include <fstream>
#include "Entity.h"
#include "Component.h"

namespace IKore {

    /**
     * @brief Performance metrics for tracking ECS system performance
     */
    struct PerformanceMetrics {
        // Timing metrics (in microseconds)
        double entityCreationTime = 0.0;
        double componentAddTime = 0.0;
        double componentRemovalTime = 0.0;
        double componentLookupTime = 0.0;
        double entityIterationTime = 0.0;
        double systemUpdateTime = 0.0;
        double renderTime = 0.0;
        
        // Memory metrics (in bytes)
        size_t entityMemoryUsage = 0;
        size_t componentMemoryUsage = 0;
        size_t totalMemoryUsage = 0;
        
        // Count metrics
        size_t entityCount = 0;
        size_t componentCount = 0;
        size_t activeEntities = 0;
        size_t inactiveEntities = 0;
        
        // Performance ratios
        double entitiesPerSecond = 0.0;
        double componentsPerSecond = 0.0;
        double lookupsPerSecond = 0.0;
        double updatesPerSecond = 0.0;
    };

    /**
     * @brief Benchmark configuration for customizable testing
     */
    struct BenchmarkConfig {
        size_t entityCounts[5] = {100, 500, 1000, 5000, 10000};
        size_t componentTypesPerEntity = 3;
        size_t iterations = 1000;
        size_t lookupIterations = 10000;
        bool enableMemoryTracking = true;
        bool enableDetailedLogging = false;
        bool outputToFile = true;
        std::string outputFileName = "ecs_benchmark_results.txt";
    };

    /**
     * @brief ECS Performance Benchmarking System
     * 
     * This system provides comprehensive performance testing for the Entity-Component System:
     * - Memory usage tracking for entities and components
     * - Performance measurement for all ECS operations
     * - Scalability testing with varying entity counts
     * - Detailed reporting and analysis
     * 
     * Usage Examples:
     * 
     * @code
     * // Basic performance test
     * ECSBenchmark benchmark;
     * auto metrics = benchmark.runComprehensiveBenchmark();
     * 
     * // Custom configuration
     * BenchmarkConfig config;
     * config.entityCounts[0] = 2000;
     * config.iterations = 5000;
     * auto results = benchmark.runBenchmarkWithConfig(config);
     * 
     * // Real-time monitoring
     * benchmark.startRealTimeMonitoring();
     * // ... run your application
     * benchmark.stopRealTimeMonitoring();
     * benchmark.generateReport();
     * @endcode
     */
    class ECSBenchmark {
    public:
        ECSBenchmark();
        ~ECSBenchmark();

        // Main benchmark functions
        PerformanceMetrics runComprehensiveBenchmark();
        PerformanceMetrics runBenchmarkWithConfig(const BenchmarkConfig& config);
        std::vector<PerformanceMetrics> runScalabilityTest();
        
        // Specific performance tests
        PerformanceMetrics benchmarkEntityOperations(size_t entityCount);
        PerformanceMetrics benchmarkComponentOperations(size_t entityCount);
        PerformanceMetrics benchmarkLookupPerformance(size_t entityCount, size_t lookupCount);
        PerformanceMetrics benchmarkIterationPerformance(size_t entityCount);
        PerformanceMetrics benchmarkMemoryUsage(size_t entityCount);
        
        // Real-time monitoring
        void startRealTimeMonitoring();
        void stopRealTimeMonitoring();
        PerformanceMetrics getCurrentMetrics() const;
        
        // Stress testing
        PerformanceMetrics stressTestEntityCreation(size_t maxEntities);
        PerformanceMetrics stressTestComponentManagement(size_t entityCount, size_t operationsCount);
        PerformanceMetrics stressTestConcurrentAccess(size_t entityCount, size_t threadCount);
        
        // Memory profiling
        void enableMemoryProfiling(bool enable);
        void trackMemoryAllocation(size_t bytes, const std::string& source);
        void trackMemoryDeallocation(size_t bytes, const std::string& source);
        size_t getCurrentMemoryUsage() const;
        size_t getPeakMemoryUsage() const;
        
        // Component-specific benchmarks
        template<typename T>
        PerformanceMetrics benchmarkComponentType(size_t entityCount);
        
        // Reporting and analysis
        void generateReport(const std::vector<PerformanceMetrics>& results = {});
        void generateDetailedReport(const std::vector<PerformanceMetrics>& results);
        void exportToCsv(const std::vector<PerformanceMetrics>& results, const std::string& filename);
        void exportToJson(const std::vector<PerformanceMetrics>& results, const std::string& filename);
        
        // Performance validation
        bool validatePerformance(const PerformanceMetrics& metrics, const BenchmarkConfig& config);
        bool isPerformanceAcceptable(const PerformanceMetrics& metrics);
        std::vector<std::string> getPerformanceWarnings(const PerformanceMetrics& metrics);
        
        // Configuration
        void setConfig(const BenchmarkConfig& config);
        BenchmarkConfig getConfig() const;
        void resetBenchmark();

    private:
        // Internal benchmark helpers
        std::vector<std::shared_ptr<Entity>> createTestEntities(size_t count);
        void addRandomComponents(std::shared_ptr<Entity> entity, size_t componentCount);
        void cleanupTestEntities(std::vector<std::shared_ptr<Entity>>& entities);
        
        // Timing utilities
        class Timer {
        public:
            Timer() : start_(std::chrono::high_resolution_clock::now()) {}
            
            void reset() {
                start_ = std::chrono::high_resolution_clock::now();
            }
            
            double elapsedMicroseconds() const {
                auto end = std::chrono::high_resolution_clock::now();
                return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
            }
            
            double elapsedMilliseconds() const {
                return elapsedMicroseconds() / 1000.0;
            }
            
        private:
            std::chrono::high_resolution_clock::time_point start_;
        };
        
        // Memory tracking
        struct MemoryTracker {
            size_t currentUsage = 0;
            size_t peakUsage = 0;
            std::unordered_map<std::string, size_t> sourceUsage;
            
            void allocate(size_t bytes, const std::string& source) {
                currentUsage += bytes;
                sourceUsage[source] += bytes;
                if (currentUsage > peakUsage) {
                    peakUsage = currentUsage;
                }
            }
            
            void deallocate(size_t bytes, const std::string& source) {
                currentUsage = (currentUsage > bytes) ? currentUsage - bytes : 0;
                sourceUsage[source] = (sourceUsage[source] > bytes) ? sourceUsage[source] - bytes : 0;
            }
        };
        
        // Data members
        BenchmarkConfig m_config;
        MemoryTracker m_memoryTracker;
        bool m_memoryProfilingEnabled;
        bool m_realTimeMonitoring;
        PerformanceMetrics m_currentMetrics;
        std::vector<PerformanceMetrics> m_historicalMetrics;
        
        // Helper methods
        void updateCurrentMetrics();
        void logMetrics(const PerformanceMetrics& metrics, const std::string& testName);
        std::string formatMetrics(const PerformanceMetrics& metrics);
        void writeToFile(const std::string& content, const std::string& filename);
        
        // Statistical analysis
        double calculateAverage(const std::vector<double>& values);
        double calculateStandardDeviation(const std::vector<double>& values);
        PerformanceMetrics calculateAverageMetrics(const std::vector<PerformanceMetrics>& metrics);
        
        // Performance thresholds
        static constexpr double MAX_ENTITY_CREATION_TIME_US = 100.0;  // 100 microseconds per entity
        static constexpr double MAX_COMPONENT_ADD_TIME_US = 50.0;     // 50 microseconds per component
        static constexpr double MAX_LOOKUP_TIME_US = 10.0;           // 10 microseconds per lookup
        static constexpr double MAX_ITERATION_TIME_US = 1000.0;      // 1 millisecond for 1000 entities
        static constexpr size_t MAX_MEMORY_PER_ENTITY_BYTES = 1024;  // 1KB per entity
    };

    // Template implementation for component-specific benchmarks
    template<typename T>
    PerformanceMetrics ECSBenchmark::benchmarkComponentType(size_t entityCount) {
        PerformanceMetrics metrics;
        Timer timer;
        
        // Create test entities
        auto entities = createTestEntities(entityCount);
        
        // Benchmark component addition
        timer.reset();
        for (auto& entity : entities) {
            entity->addComponent<T>();
        }
        metrics.componentAddTime = timer.elapsedMicroseconds() / entityCount;
        
        // Benchmark component lookup
        timer.reset();
        for (size_t i = 0; i < m_config.lookupIterations; ++i) {
            for (auto& entity : entities) {
                auto component = entity->getComponent<T>();
                (void)component; // Suppress unused variable warning
            }
        }
        metrics.componentLookupTime = timer.elapsedMicroseconds() / (entityCount * m_config.lookupIterations);
        
        // Benchmark component removal
        timer.reset();
        for (auto& entity : entities) {
            entity->removeComponent<T>();
        }
        metrics.componentRemovalTime = timer.elapsedMicroseconds() / entityCount;
        
        metrics.entityCount = entityCount;
        metrics.componentCount = entityCount; // One component per entity
        
        cleanupTestEntities(entities);
        return metrics;
    }

} // namespace IKore
