#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <thread>
#include <chrono>

#include "src/core/EventSystem.h"

using namespace IKore;

// Forward declaration for Entity to avoid dependency
class Entity;

// Test event data types
struct CollisionEvent {
    std::string objectA;
    std::string objectB;
    float impactForce;
};

struct DamageEvent {
    float amount;
    std::string source;
};

// Simple test class 
class TestObject {
public:
    TestObject(const std::string& name) : m_name(name) {}
    
    void initialize() {
        // Subscribe to events
        m_collisionSubscription = EventSystem::getInstance().subscribe("collision", 
            [this](const std::shared_ptr<EventData>& data) {
                auto typedData = std::dynamic_pointer_cast<TypedEventData<CollisionEvent>>(data);
                if (typedData) {
                    this->onCollision(typedData->data);
                }
            });
            
        m_damageSubscription = EventSystem::getInstance().subscribe("damage", 
            [this](const std::shared_ptr<EventData>& data) {
                auto typedData = std::dynamic_pointer_cast<TypedEventData<DamageEvent>>(data);
                if (typedData) {
                    this->onDamage(typedData->data);
                }
            });
    }
    
    void cleanup() {
        // Unsubscribe from events
        EventSystem::getInstance().unsubscribe(m_collisionSubscription);
        EventSystem::getInstance().unsubscribe(m_damageSubscription);
    }
    
    void onCollision(const CollisionEvent& event) {
        if (event.objectA == m_name || event.objectB == m_name) {
            m_collisions.push_back(event);
            std::cout << m_name << " received collision event with force: " << event.impactForce << std::endl;
        }
    }
    
    void onDamage(const DamageEvent& event) {
        m_damageReceived += event.amount;
        std::cout << m_name << " received damage: " << event.amount << " from " << event.source << std::endl;
    }
    
    const std::vector<CollisionEvent>& getCollisions() const { return m_collisions; }
    float getDamageReceived() const { return m_damageReceived; }
    
private:
    std::string m_name;
    std::vector<CollisionEvent> m_collisions;
    float m_damageReceived = 0.0f;
    EventSubscription m_collisionSubscription;
    EventSubscription m_damageSubscription;
};

int main() {
    std::cout << "=== Event System Test ===" << std::endl;
    
    // Create test objects
    TestObject player("Player");
    TestObject enemy("Enemy");
    TestObject wall("Wall");
    
    // Initialize objects (subscribe to events)
    player.initialize();
    enemy.initialize();
    wall.initialize();
    
    // Publish a collision event
    CollisionEvent collision1 = {"Player", "Wall", 50.0f};
    EventSystem::getInstance().publish("collision", collision1);
    
    // Publish a damage event
    DamageEvent damage1 = {25.0f, "Enemy"};
    EventSystem::getInstance().publish("damage", damage1);
    
    // Verify player received collision
    assert(player.getCollisions().size() == 1);
    assert(player.getCollisions()[0].impactForce == 50.0f);
    
    // Verify player received damage
    assert(player.getDamageReceived() == 25.0f);
    
    // Test unsubscribing by cleaning up the wall object
    wall.cleanup();
    
    // Publish another collision that wall shouldn't receive
    CollisionEvent collision2 = {"Player", "Wall", 75.0f};
    EventSystem::getInstance().publish("collision", collision2);
    
    // Verify wall didn't receive the second collision
    assert(wall.getCollisions().size() == 1);
    
    // Test with many events for performance
    std::cout << "\nTesting performance with many events..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    constexpr int NUM_EVENTS = 10000;
    for (int i = 0; i < NUM_EVENTS; i++) {
        DamageEvent smallDamage = {0.1f, "Performance Test"};
        EventSystem::getInstance().publish("damage", smallDamage);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Published " << NUM_EVENTS << " events in " << duration << "ms" << std::endl;
    std::cout << "Average: " << (float)NUM_EVENTS / duration * 1000 << " events/second" << std::endl;
    
    // Clean up remaining objects
    player.cleanup();
    enemy.cleanup();
    
    // Clear the event system
    EventSystem::getInstance().clear();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
