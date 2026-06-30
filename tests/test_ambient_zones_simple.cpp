#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>

#include "audio/AmbientSoundZone.h"

using namespace IKore;

/**
 * @brief Test program for Ambient Sound Zones
 * 
 * This program tests the basic functionality of ambient sound zones:
 * - Zone creation and basic properties
 * - Distance-based volume calculation
 * - Zone detection (position in zone)
 * - Simple zone management
 */

/**
 * @brief Test basic ambient zone creation and properties
 */
void testAmbientZoneCreation() {
    std::cout << "\n=== Testing Ambient Zone Creation ===" << std::endl;
    
    // Create a forest zone
    AmbientSoundZone forestZone;
    forestZone.name = "Forest";
    forestZone.center = glm::vec3(0.0f, 0.0f, 0.0f);
    forestZone.radius = 15.0f;
    forestZone.audioFile = "audio/forest_ambient.wav";
    forestZone.volume = 0.8f;
    forestZone.priority = 1;
    
    // Create a cave zone  
    AmbientSoundZone caveZone;
    caveZone.name = "Cave";
    caveZone.center = glm::vec3(20.0f, 0.0f, 0.0f);
    caveZone.radius = 10.0f;
    caveZone.audioFile = "audio/cave_ambient.wav";
    caveZone.volume = 0.6f;
    caveZone.priority = 2;
    
    // Test basic properties
    assert(forestZone.name == "Forest");
    assert(forestZone.radius == 15.0f);
    assert(forestZone.volume == 0.8f);
    assert(forestZone.priority == 1);
    
    std::cout << "✅ Created ambient zones: " << forestZone.name << " and " << caveZone.name << std::endl;
    
    // Test position-based functionality
    glm::vec3 centerPos(0.0f, 0.0f, 0.0f);
    glm::vec3 edgePos(14.0f, 0.0f, 0.0f);  // Near edge of forest zone
    glm::vec3 outsidePos(30.0f, 0.0f, 0.0f); // Outside both zones
    
    // Test isPositionInZone
    assert(forestZone.isPositionInZone(centerPos) == true);
    assert(forestZone.isPositionInZone(edgePos) == true);
    assert(forestZone.isPositionInZone(outsidePos) == false);
    
    std::cout << "✅ Zone position detection working correctly" << std::endl;
    
    // Test volume calculation
    float centerVolume = forestZone.getVolumeAtPosition(centerPos);
    float edgeVolume = forestZone.getVolumeAtPosition(edgePos);
    float outsideVolume = forestZone.getVolumeAtPosition(outsidePos);
    
    assert(centerVolume == 0.8f);  // Full volume at center
    assert(edgeVolume > 0.0f && edgeVolume <= 0.8f);  // Some volume near edge
    assert(outsideVolume == 0.0f);  // No volume outside
    
    std::cout << "✅ Volume calculation working correctly" << std::endl;
    std::cout << "   Center volume: " << centerVolume << std::endl;
    std::cout << "   Edge volume: " << edgeVolume << std::endl;
    std::cout << "   Outside volume: " << outsideVolume << std::endl;
}

/**
 * @brief Test zone manager functionality
 */
void testZoneManager() {
    std::cout << "\n=== Testing Zone Manager ===" << std::endl;
    
    // Create a simple audio system pointer (null for this test)
    AmbientSoundZoneManager manager(nullptr);
    
    // Create test zones
    AmbientSoundZone zone1;
    zone1.name = "Zone1";
    zone1.center = glm::vec3(0.0f, 0.0f, 0.0f);
    zone1.radius = 10.0f;
    zone1.volume = 0.7f;
    zone1.priority = 1;
    
    AmbientSoundZone zone2;
    zone2.name = "Zone2";
    zone2.center = glm::vec3(15.0f, 0.0f, 0.0f);
    zone2.radius = 8.0f;
    zone2.volume = 0.5f;
    zone2.priority = 2;
    
    // Add zones to manager
    manager.addZone(zone1);
    manager.addZone(zone2);
    
    // Verify zones were added
    assert(manager.getZones().size() == 2);
    std::cout << "✅ Zone manager holds " << manager.getZones().size() << " zones" << std::endl;
    
    // Test clear functionality
    manager.clearZones();
    assert(manager.getZones().size() == 0);
    assert(manager.getCurrentZone() == nullptr);
    std::cout << "✅ Zone manager cleared successfully" << std::endl;
}

/**
 * @brief Test overlapping zones functionality
 */
void testOverlappingZones() {
    std::cout << "\n=== Testing Overlapping Zones ===" << std::endl;
    
    // Create overlapping zones
    AmbientSoundZone largeZone;
    largeZone.name = "Large Zone";
    largeZone.center = glm::vec3(0.0f, 0.0f, 0.0f);
    largeZone.radius = 20.0f;
    largeZone.volume = 0.4f;
    largeZone.priority = 1;
    
    AmbientSoundZone smallZone;
    smallZone.name = "Small Zone";
    smallZone.center = glm::vec3(5.0f, 0.0f, 0.0f);
    smallZone.radius = 8.0f;
    smallZone.volume = 0.8f;
    smallZone.priority = 2;  // Higher priority
    
    // Test position that's in both zones
    glm::vec3 overlapPos(5.0f, 0.0f, 0.0f);  // Center of small zone
    
    assert(largeZone.isPositionInZone(overlapPos) == true);
    assert(smallZone.isPositionInZone(overlapPos) == true);
    
    float largeZoneVolume = largeZone.getVolumeAtPosition(overlapPos);
    float smallZoneVolume = smallZone.getVolumeAtPosition(overlapPos);
    
    std::cout << "✅ Overlapping zones detected correctly" << std::endl;
    std::cout << "   Large zone volume at overlap: " << largeZoneVolume << std::endl;
    std::cout << "   Small zone volume at overlap: " << smallZoneVolume << std::endl;
    std::cout << "   Small zone has higher priority: " << smallZone.priority << " > " << largeZone.priority << std::endl;
}

/**
 * @brief Test performance with many zones
 */
void testPerformance() {
    std::cout << "\n=== Testing Performance ===" << std::endl;
    
    AmbientSoundZoneManager manager(nullptr);
    
    // Create many zones
    const int numZones = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numZones; ++i) {
        AmbientSoundZone zone;
        zone.name = "Zone" + std::to_string(i);
        zone.center = glm::vec3(i * 5.0f, 0.0f, 0.0f);
        zone.radius = 10.0f;
        zone.volume = 0.5f;
        zone.priority = i % 5;  // Varying priorities
        manager.addZone(zone);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // Test update performance
    glm::vec3 testPosition(25.0f, 0.0f, 0.0f);
    for (int i = 0; i < 1000; ++i) {
        manager.update(testPosition, 0.016f);  // 60 FPS
        testPosition.x += 0.1f;  // Move slightly each update
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto setupTime = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    std::cout << "✅ Performance test completed" << std::endl;
    std::cout << "   Setup time for " << numZones << " zones: " << setupTime.count() << " μs" << std::endl;
    std::cout << "   Update time for 1000 iterations: " << updateTime.count() << " μs" << std::endl;
    std::cout << "   Average update time: " << (updateTime.count() / 1000.0) << " μs per update" << std::endl;
    
    // Verify we still have all zones
    assert(manager.getZones().size() == numZones);
}

int main() {
    std::cout << "🎵 Ambient Sound Zones Test Suite 🎵" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    try {
        testAmbientZoneCreation();
        testZoneManager();
        testOverlappingZones();
        testPerformance();
        
        std::cout << "\n🎉 All tests passed! Ambient Sound Zones are working correctly." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
