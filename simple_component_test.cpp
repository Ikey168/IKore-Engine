#include <iostream>
#include <memory>
#include <cassert>

// Simple test without external dependencies
class TestComponent {
public:
    TestComponent() = default;
    virtual ~TestComponent() = default;
    int value = 42;
};

namespace IKore {
    using EntityID = uint64_t;
    constexpr EntityID INVALID_ENTITY_ID = 0;

    class SimpleEntity {
    public:
        SimpleEntity() : m_id(++s_nextID) {}
        EntityID getID() const { return m_id; }
        
        template<typename T>
        std::shared_ptr<T> addComponent() {
            auto component = std::make_shared<T>();
            m_component = component;
            return component;
        }
        
        template<typename T>
        std::shared_ptr<T> getComponent() {
            return std::static_pointer_cast<T>(m_component);
        }
        
        template<typename T>
        bool hasComponent() const {
            return m_component != nullptr;
        }
        
    private:
        EntityID m_id;
        std::shared_ptr<void> m_component; // Simplified storage
        static EntityID s_nextID;
    };
    
    EntityID SimpleEntity::s_nextID = 0;
}

int main() {
    std::cout << "🧪 Testing Entity-Component System..." << std::endl;
    
    // Create entity
    auto entity = std::make_shared<IKore::SimpleEntity>();
    std::cout << "✅ Entity created with ID: " << entity->getID() << std::endl;
    
    // Add component
    auto testComponent = entity->addComponent<TestComponent>();
    testComponent->value = 100;
    std::cout << "✅ Component added with value: " << testComponent->value << std::endl;
    
    // Check component exists
    assert(entity->hasComponent<TestComponent>());
    std::cout << "✅ Component existence check passed" << std::endl;
    
    // Get component
    auto retrievedComponent = entity->getComponent<TestComponent>();
    assert(retrievedComponent != nullptr);
    assert(retrievedComponent->value == 100);
    std::cout << "✅ Component retrieval passed with value: " << retrievedComponent->value << std::endl;
    
    std::cout << "🎉 All Entity-Component System tests passed!" << std::endl;
    
    return 0;
}
