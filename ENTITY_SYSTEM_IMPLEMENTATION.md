# Entity System Implementation

## Issue #22: Implement Base Entity System

### Overview
This document describes the complete implementation of a base Entity System for the IKore Engine. The implementation provides a foundation for managing game objects with unique identifiers, dynamic creation/deletion, and comprehensive iteration capabilities.

### Features Implemented

#### ✅ Core Entity System
- **Base Entity class** with unique ID generation
- **EntityManager singleton** for global entity management
- **Dynamic entity creation and removal** at runtime
- **Type-safe templated entity operations**
- **Thread-safe operations** with mutex protection
- **Comprehensive lifecycle management** (initialize, update, cleanup)

#### ✅ Unique ID System
- **Atomic ID generation** ensures thread-safe unique identifiers
- **64-bit EntityID type** supports massive entity counts
- **ID persistence** throughout entity lifetime
- **Invalid ID handling** with INVALID_ENTITY_ID constant
- **ID validation** methods for entity state checking

#### ✅ Entity Management
- **Global EntityManager singleton** accessible throughout the engine
- **Template-based entity creation** with perfect forwarding
- **Smart pointer management** with std::shared_ptr
- **Automatic entity registration** during creation
- **Safe entity removal** with proper cleanup

#### ✅ Iteration and Access
- **Get all entities** with getAllEntities()
- **Type-specific filtering** with getEntitiesOfType<T>()
- **Callback-based iteration** with forEach()
- **Individual entity lookup** by ID
- **Existence checking** with hasEntity()

### Technical Architecture

#### Entity Class Hierarchy
```cpp
class Entity {
    // Base class with unique ID
    EntityID getID() const;
    virtual void update(float deltaTime);
    virtual void initialize();
    virtual void cleanup();
};

class GameObject : public Entity {
    // 3D object with transform properties
    glm::vec3 position, rotation, scale;
    std::string name;
    bool visible, active;
};

class LightEntity : public GameObject {
    // Light source with lighting properties
    LightType type;
    glm::vec3 color;
    float intensity, range;
};

class CameraEntity : public GameObject {
    // Camera with view/projection properties
    float fieldOfView, aspectRatio;
    float nearPlane, farPlane;
};

class TestEntity : public Entity {
    // Testing entity with lifetime management
    std::string message;
    float lifetime, age;
};
```

#### EntityManager Interface
```cpp
class EntityManager {
    // Singleton pattern
    static EntityManager& getInstance();
    
    // Template-based creation
    template<typename T, typename... Args>
    std::shared_ptr<T> createEntity(Args&&... args);
    
    // Entity management
    void addEntity(std::shared_ptr<Entity> entity);
    bool removeEntity(EntityID id);
    std::shared_ptr<Entity> getEntity(EntityID id);
    
    // Iteration and filtering
    std::vector<std::shared_ptr<Entity>> getAllEntities();
    template<typename T>
    std::vector<std::shared_ptr<T>> getEntitiesOfType();
    void forEach(std::function<void(std::shared_ptr<Entity>)> callback);
    
    // Lifecycle management
    void updateAll(float deltaTime);
    void initializeAll();
    void cleanupAll();
    
    // Statistics and utilities
    EntityStats getStats();
    size_t getEntityCount();
    bool hasEntity(EntityID id);
    void clear();
};
```

### Implementation Details

#### Unique ID Generation
```cpp
// Thread-safe atomic counter
static std::atomic<EntityID> s_nextID{1};

EntityID Entity::generateID() {
    return s_nextID.fetch_add(1, std::memory_order_relaxed);
}
```

The system uses an atomic counter starting from 1 (0 is reserved for INVALID_ENTITY_ID) to ensure each entity gets a unique identifier even in multi-threaded environments.

#### Thread Safety
```cpp
class EntityManager {
private:
    std::unordered_map<EntityID, std::shared_ptr<Entity>> m_entities;
    mutable std::mutex m_mutex; // Protects all operations
};
```

All EntityManager operations are protected by a mutex to ensure thread safety when accessing or modifying the entity collection.

#### Template-Based Creation
```cpp
template<typename T, typename... Args>
std::shared_ptr<T> createEntity(Args&&... args) {
    static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
    
    auto entity = std::make_shared<T>(std::forward<Args>(args)...);
    addEntity(entity);
    return entity;
}
```

Type-safe entity creation with compile-time checks and perfect forwarding of constructor arguments.

### File Structure

#### New Files Added
```
src/Entity.h                 - Core Entity and EntityManager declarations
src/Entity.cpp               - Entity system implementation
src/EntityTypes.h            - Example entity type declarations
src/EntityTypes.cpp          - Example entity type implementations
test_entity_system.sh        - Comprehensive testing script
```

#### Modified Files
```
src/main.cpp                 - Entity system integration and testing
CMakeLists.txt               - Added Entity.cpp and EntityTypes.cpp to build
```

### Entity System Integration

#### Main Application Integration
```cpp
// Initialization
IKore::EntityManager& entityManager = IKore::getEntityManager();

// Create test entities
auto testEntity = entityManager.createEntity<IKore::TestEntity>("Test Message");
auto gameObject = entityManager.createEntity<IKore::GameObject>("Object 1", glm::vec3(1,2,3));
auto light = entityManager.createEntity<IKore::LightEntity>("Main Light", LightType::DIRECTIONAL);

// Initialize all entities
entityManager.initializeAll();

// Main loop update
entityManager.updateAll(deltaTime);

// Cleanup on shutdown
entityManager.cleanupAll();
entityManager.clear();
```

#### Example Entity Creation
```cpp
// Create GameObject with transform
auto gameObject = entityManager.createEntity<GameObject>("Player", glm::vec3(0, 0, 0));
gameObject->setRotation(glm::vec3(0, 45, 0));
gameObject->setScale(glm::vec3(2, 1, 1));

// Create Light with properties
auto light = entityManager.createEntity<LightEntity>("Sun", LightEntity::LightType::DIRECTIONAL);
light->setPosition(glm::vec3(0, 10, 0));
light->setColor(glm::vec3(1.0f, 0.9f, 0.8f));
light->setIntensity(1.5f);

// Create Camera
auto camera = entityManager.createEntity<CameraEntity>("Main Camera");
camera->setPosition(glm::vec3(0, 0, 5));
camera->setFieldOfView(60.0f);
```

### Entity Types Implemented

#### GameObject
- **Transform system** with position, rotation, scale
- **Visibility and active state** management
- **Name identification** for debugging
- **Transform matrix generation** for rendering
- **Transform manipulation** methods (translate, rotate, scale)

#### LightEntity
- **Multiple light types** (Directional, Point, Spot)
- **Color and intensity** control
- **Range and spot angle** parameters
- **Integration ready** for lighting systems

#### CameraEntity
- **View and projection matrices** generation
- **Camera parameters** (FOV, aspect ratio, near/far planes)
- **Direction vectors** calculation (forward, right, up)
- **Integration ready** for rendering systems

#### TestEntity
- **Message-based identification** for testing
- **Lifetime management** with automatic expiration
- **Age tracking** for debugging and profiling

### Interactive Controls

#### Keyboard Controls
- **E key**: Display entity system statistics and entity counts by type
- **R key**: Remove all test entities to demonstrate dynamic removal
- **T key**: Create new test entities with varying lifetimes
- **Real-time updates**: Entity system updates automatically each frame

#### Entity Statistics
```cpp
struct EntityStats {
    size_t totalEntities;     // Total entities managed
    size_t validEntities;     // Currently valid entities
    size_t invalidEntities;   // Invalidated entities
    EntityID highestID;       // Highest ID assigned
    EntityID lowestID;        // Lowest ID in use
};
```

### Performance Characteristics

#### Memory Management
- **Smart pointer usage** prevents memory leaks
- **Efficient lookup** with std::unordered_map (O(1) average)
- **Minimal overhead** per entity (~8 bytes for ID + shared_ptr overhead)
- **Automatic cleanup** when entities go out of scope

#### Scalability
- **64-bit IDs** support up to 18 quintillion entities
- **Atomic operations** ensure thread-safe ID generation
- **Template specialization** allows optimized type-specific operations
- **Batch operations** for initialization, update, and cleanup

#### Thread Safety
- **Mutex protection** on all critical operations
- **Atomic ID generation** prevents race conditions
- **Safe iteration** with lock guards
- **Exception safety** with RAII principles

### Usage Examples

#### Basic Entity Creation
```cpp
auto entity = entityManager.createEntity<TestEntity>("Hello World");
entity->setLifetime(5.0f);
entity->initialize();
```

#### Entity Removal
```cpp
// Remove by ID
EntityID id = entity->getID();
entityManager.removeEntity(id);

// Remove by pointer
entityManager.removeEntity(entity);
```

#### Entity Iteration
```cpp
// Iterate all entities
entityManager.forEach([](std::shared_ptr<Entity> entity) {
    std::cout << "Entity ID: " << entity->getID() << std::endl;
});

// Iterate specific type
auto gameObjects = entityManager.getEntitiesOfType<GameObject>();
for (auto& obj : gameObjects) {
    obj->update(deltaTime);
}
```

#### Entity Lookup
```cpp
// Get entity by ID
auto entity = entityManager.getEntity(id);
if (entity) {
    entity->update(deltaTime);
}

// Get typed entity
auto gameObject = entityManager.getEntity<GameObject>(id);
if (gameObject) {
    gameObject->setPosition(glm::vec3(1, 2, 3));
}
```

### Acceptance Criteria Verification

#### ✅ Entities can be created and removed dynamically
- **Template-based creation** allows any entity type to be created at runtime
- **Dynamic removal** by ID or pointer with immediate cleanup
- **Safe lifecycle management** ensures proper initialization and cleanup
- **Interactive testing** with T and R keys demonstrates dynamic operations

#### ✅ Each entity has a unique ID
- **Atomic ID generation** ensures uniqueness across all threads
- **64-bit EntityID type** prevents ID exhaustion
- **ID validation** methods check entity validity
- **Persistent IDs** remain constant throughout entity lifetime

#### ✅ Entity list can be accessed and iterated
- **getAllEntities()** returns complete entity list
- **getEntitiesOfType<T>()** filters by entity type
- **forEach()** provides callback-based iteration
- **Thread-safe iteration** with proper locking

### Testing and Validation

#### Automated Testing
The `test_entity_system.sh` script validates:
- ✅ Build system integration
- ✅ Implementation completeness
- ✅ File structure correctness
- ✅ Runtime functionality
- ✅ Interactive controls

#### Manual Testing Scenarios
1. **Entity creation and removal** - Test dynamic entity management
2. **ID uniqueness** - Verify no duplicate IDs are generated
3. **Type filtering** - Confirm type-specific entity retrieval
4. **Lifecycle management** - Test initialize, update, cleanup calls
5. **Thread safety** - Verify safe concurrent access

### Performance Results

#### Before Entity System
- **No centralized entity management**
- **Manual object lifecycle tracking**
- **No standardized entity identification**

#### After Entity System
- **Centralized entity management** with EntityManager
- **Automatic lifecycle management** for all entities
- **Unique ID system** for reliable entity identification
- **Type-safe operations** with compile-time checking
- **Thread-safe access** for multi-threaded environments

### Future Enhancements

#### Potential Improvements
- **Component System** integration for data-oriented design
- **Entity serialization** for save/load functionality
- **Spatial partitioning** for efficient spatial queries
- **Event system** for entity communication
- **Pooling** for frequently created/destroyed entities

#### Performance Optimizations
- **Memory pooling** for reduced allocation overhead
- **Batch operations** for improved cache locality
- **Lock-free data structures** for high-performance scenarios
- **SIMD optimizations** for bulk entity operations

### Implementation Statistics

#### Code Metrics
- **Entity.h**: 300+ lines (comprehensive entity management API)
- **Entity.cpp**: 200+ lines (complete implementation)
- **EntityTypes.h**: 150+ lines (example entity types)
- **EntityTypes.cpp**: 200+ lines (entity type implementations)
- **Total addition**: 850+ lines of entity system code

#### Technical Achievements
- ✅ **Thread-safe unique ID generation** with atomic operations
- ✅ **Template-based type safety** for entity operations
- ✅ **Comprehensive lifecycle management** for all entities
- ✅ **Smart pointer memory management** prevents leaks
- ✅ **Interactive testing controls** for real-time validation
- ✅ **Extensive logging integration** for debugging
- ✅ **Complete statistics tracking** for system monitoring

### Conclusion

The Entity System implementation successfully addresses all requirements from Issue #22:
- ✅ **Base Entity class with unique ID** - Implemented with atomic ID generation
- ✅ **Global entity manager** - Singleton EntityManager with comprehensive API
- ✅ **Dynamic creation and removal** - Template-based creation with safe removal
- ✅ **Entity iteration and access** - Multiple iteration methods with type filtering

The system provides a solid foundation for game object management in the IKore Engine, with excellent performance characteristics, thread safety, and extensibility for future enhancements. The comprehensive testing suite and interactive controls make it easy to validate functionality and understand system behavior.
