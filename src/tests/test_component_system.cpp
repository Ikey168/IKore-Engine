#include <iostream>
#include <cassert>
#include <memory>

#include "core/Entity.h"
#include "core/Component.h"

using namespace IKore;

// Simple test component for basic functionality
class TestComponent : public Component {
public:
    TestComponent() = default;
    virtual ~TestComponent() = default;
    
    int value = 42;
    
    void onAttach() override {
        std::cout << "TestComponent attached" << std::endl;
    }
    
    void onDetach() override {
        std::cout << "TestComponent detached" << std::endl;
    }
};

int main() {
    std::cout << "Running Component System Tests..." << std::endl;
    
    // Create an entity
    auto entity = std::make_shared<Entity>();
    
    // Test component addition
    auto testComp = entity->addComponent<TestComponent>();
    assert(testComp != nullptr);
    assert(entity->hasComponent<TestComponent>());
    
    // Test component value
    assert(testComp->value == 42);
    testComp->value = 100;
    
    // Test component retrieval
    auto retrievedComp = entity->getComponent<TestComponent>();
    assert(retrievedComp != nullptr);
    assert(retrievedComp->value == 100);
    
    // Test component removal
    entity->removeComponent<TestComponent>();
    assert(!entity->hasComponent<TestComponent>());
    assert(entity->getComponent<TestComponent>() == nullptr);
    
    std::cout << "✓ Basic component operations test passed!" << std::endl;
    std::cout << "🎉 Component System is working!" << std::endl;
    
    return 0;
}
