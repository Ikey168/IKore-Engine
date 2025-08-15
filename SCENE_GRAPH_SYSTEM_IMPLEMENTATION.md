# Scene Graph System Implementation

## Overview

The Scene Graph System for IKore Engine provides comprehensive hierarchical entity management with the following key features:

- **Hierarchical Entity Management**: Support for parent-child relationships between entities
- **Efficient Scene Traversal**: Depth-first and breadth-first traversal algorithms
- **Transform Propagation**: Automatic hierarchical transform updates
- **Visibility and State Management**: Per-node visibility and active state control
- **Performance Optimization**: Dirty marking for efficient transform updates
- **Comprehensive API**: High-level Scene Manager for convenient operations

## Core Components

### 1. SceneGraphSystem (`src/SceneGraphSystem.h`, `src/SceneGraphSystem.cpp`)

The core scene graph implementation providing:

#### Key Classes:
- `SceneNode`: Represents a node in the scene graph
  - Contains entity reference, parent/children relationships
  - Visibility and active state flags
  
- `SceneGraphSystem`: Singleton managing the entire scene graph
  - Node management (add, remove, find)
  - Hierarchy operations (set parent, get children/descendants)
  - Traversal methods (depth-first, breadth-first)
  - Transform updates with dirty marking
  - Visibility and state management

#### Core Features:
```cpp
// Entity management
std::shared_ptr<SceneNode> addEntity(std::shared_ptr<Entity> entity, std::shared_ptr<SceneNode> parent = nullptr);
bool removeEntity(std::shared_ptr<Entity> entity);
std::shared_ptr<SceneNode> findNode(EntityID entityID);

// Hierarchy management
bool setParent(std::shared_ptr<SceneNode> child, std::shared_ptr<SceneNode> parent);
std::vector<std::shared_ptr<SceneNode>> getChildren(std::shared_ptr<Entity> entity);
std::vector<std::shared_ptr<SceneNode>> getDescendants(std::shared_ptr<Entity> entity);

// Traversal
void traverseDepthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, bool rootsOnly = true);
void traverseBreadthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, bool rootsOnly = true);

// Transform updates
void updateTransforms();
void markTransformDirty(std::shared_ptr<Entity> entity);

// Visibility management
void setVisibility(std::shared_ptr<SceneNode> node, bool visible, bool recursive = false);
std::vector<std::shared_ptr<SceneNode>> getVisibleNodes();
```

### 2. SceneManager (`src/SceneManager.h`, `src/SceneManager.cpp`)

High-level scene management utilities providing:

#### Entity Creation:
```cpp
std::shared_ptr<TransformableGameObject> createGameObject(const std::string& name, std::shared_ptr<Entity> parent = nullptr);
std::shared_ptr<TransformableLight> createLight(const std::string& name, std::shared_ptr<Entity> parent = nullptr);
std::shared_ptr<TransformableCamera> createCamera(const std::string& name, std::shared_ptr<Entity> parent = nullptr);
```

#### Scene Queries:
```cpp
std::vector<std::shared_ptr<Entity>> findEntitiesByName(const std::string& name);
std::vector<std::shared_ptr<Entity>> getAllEntities();
std::vector<std::shared_ptr<TransformableGameObject>> getAllGameObjects();
```

#### Scene Presets:
```cpp
std::vector<std::shared_ptr<Entity>> createBasicScene();  // Camera, Light, Root
std::shared_ptr<TransformableGameObject> createHierarchyDemo();  // Example hierarchy
```

## Integration with Existing Systems

### Transform System Integration

The Scene Graph System integrates seamlessly with the existing Transform hierarchy:

- **Automatic Synchronization**: When entities are added to the scene graph, their Transform parent-child relationships are automatically synchronized
- **Hierarchical Updates**: Transform updates propagate through the hierarchy automatically
- **World Space Calculations**: World transforms are calculated correctly through the hierarchy

```cpp
// Setting scene graph parent automatically sets Transform parent
sceneGraph.setParent(childNode, parentNode);
// This also calls: childEntity->setParent(parentEntity.get());
```

### Entity System Integration

- **Entity Management**: Works with all existing Entity types (TransformableGameObject, TransformableLight, TransformableCamera)
- **ID-based Lookup**: Uses Entity IDs for fast node lookup
- **Memory Management**: Uses shared_ptr for safe memory management

## Performance Characteristics

### Time Complexity:
- **Add/Remove Entity**: O(1) average case
- **Find Entity**: O(1) hash map lookup
- **Set Parent**: O(1) for parent change, O(d) for descendant checks (d = depth)
- **Depth-First Traversal**: O(n) where n = number of nodes
- **Breadth-First Traversal**: O(n) where n = number of nodes
- **Transform Update**: O(dirty nodes + their descendants)

### Space Complexity:
- **Memory Overhead**: O(n) where n = number of entities
- **Additional Storage**: ~32 bytes per node for scene graph metadata

### Optimizations:
- **Dirty Transform Marking**: Only updates transforms that have changed
- **Circular Dependency Prevention**: Checks prevent infinite loops
- **Efficient Data Structures**: Hash maps for O(1) lookups, vectors for cache-friendly iteration

## Usage Examples

### Basic Scene Creation

```cpp
#include "SceneManager.h"

auto& sceneManager = SceneManager::getInstance();
sceneManager.initialize();

// Create basic scene
auto basicEntities = sceneManager.createBasicScene();

// Create custom hierarchy
auto parent = sceneManager.createGameObject("ParentObject");
auto child = sceneManager.createGameObject("ChildObject", parent);

// Set transforms
parent->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
child->getTransform().setPosition(glm::vec3(2.0f, 0.0f, 0.0f));

sceneManager.updateTransforms();
```

### Scene Traversal

```cpp
// Visit all entities
sceneManager.forEachEntity([](std::shared_ptr<Entity> entity) {
    auto transformable = std::dynamic_pointer_cast<TransformableGameObject>(entity);
    if (transformable) {
        std::cout << "Entity: " << transformable->getName() << std::endl;
    }
});

// Advanced traversal with scene graph
auto& sceneGraph = SceneGraphSystem::getInstance();
sceneGraph.traverseDepthFirst([](std::shared_ptr<SceneNode> node) {
    if (node->isVisible && node->isActive) {
        // Process visible, active nodes
    }
});
```

### Visibility Management

```cpp
// Hide an entire subtree
sceneManager.setVisibility(parentEntity, false, true);  // recursive

// Get only visible entities for rendering
auto visibleEntities = sceneManager.getVisibleEntities();
for (auto& entity : visibleEntities) {
    // Render entity
}
```

## Testing and Validation

### Test Coverage

The implementation includes comprehensive testing:

1. **Basic Functionality Tests**:
   - Entity addition/removal
   - Node finding by ID and name
   - Basic hierarchy operations

2. **Hierarchy Management Tests**:
   - Parent-child relationships
   - Circular dependency prevention
   - Transform propagation
   - Reparenting operations

3. **Traversal Tests**:
   - Depth-first and breadth-first traversal
   - Traversal from specific nodes
   - Visitor pattern functionality

4. **Visibility and State Tests**:
   - Per-node visibility control
   - Recursive state changes
   - Filtered queries

5. **Performance Tests**:
   - Large scene handling (1000+ entities)
   - Transform update performance
   - Memory usage validation

6. **Edge Case Tests**:
   - Null entity handling
   - Deep hierarchies (100+ levels)
   - Duplicate entity addition
   - Complex reparenting scenarios

### Validation Tools

```cpp
// Scene integrity validation
bool isValid = sceneManager.validateSceneIntegrity();

// Performance statistics
std::string stats = sceneManager.getSceneStats();

// Debug output
sceneManager.printScene(true);  // detailed information
```

## Error Handling

### Robust Error Handling:
- **Null Pointer Safety**: All methods handle null entity pointers gracefully
- **Circular Dependency Prevention**: Automatic detection and prevention of circular parent-child relationships
- **Memory Safety**: Uses smart pointers throughout for automatic memory management
- **State Validation**: Built-in integrity checking to detect inconsistent state

### Logging:
- **Comprehensive Logging**: All major operations are logged with appropriate log levels
- **Debug Information**: Detailed logging for troubleshooting hierarchy issues
- **Performance Monitoring**: Log messages for performance-critical operations

## File Structure

```
src/
├── SceneGraphSystem.h          # Core scene graph interface
├── SceneGraphSystem.cpp        # Core scene graph implementation
├── SceneManager.h              # High-level scene management interface
├── SceneManager.cpp            # High-level scene management implementation
├── test_scene_graph.cpp        # Comprehensive test suite
├── scene_graph_demo.cpp        # Integration example and demo
└── simple_scene_test.cpp       # Minimal functionality test
```

## Build Integration

The Scene Graph System is integrated into the CMake build system:

```cmake
# Scene Graph Test executable
add_executable(test_scene_graph src/test_scene_graph.cpp src/Logger.cpp src/Entity.cpp 
               src/EntityTypes.cpp src/Transform.cpp src/TransformableEntities.cpp 
               src/SceneGraphSystem.cpp src/SceneManager.cpp)

# Scene Graph Demo executable  
add_executable(scene_graph_demo src/scene_graph_demo.cpp src/Logger.cpp src/Entity.cpp 
               src/EntityTypes.cpp src/Transform.cpp src/TransformableEntities.cpp 
               src/SceneGraphSystem.cpp src/SceneManager.cpp)
```

## Future Enhancements

### Potential Improvements:
1. **Spatial Queries**: Octree/Quadtree integration for spatial scene queries
2. **Serialization**: Scene graph serialization/deserialization support
3. **Multi-threading**: Thread-safe operations for concurrent scene access
4. **Component System**: Integration with Entity-Component-System (ECS) architecture
5. **Frustum Culling**: Integration with frustum culling for rendering optimization
6. **Animation Support**: Keyframe animation system integration
7. **Physics Integration**: Automatic physics world synchronization

### Performance Optimizations:
1. **Memory Pool**: Custom memory allocators for scene nodes
2. **Update Batching**: Batch transform updates for better cache performance
3. **SIMD Operations**: Vectorized transform calculations
4. **GPU Acceleration**: GPU-based hierarchy updates for large scenes

## Conclusion

The Scene Graph System provides a robust, efficient, and feature-complete solution for hierarchical entity management in the IKore Engine. It integrates seamlessly with existing systems while providing significant new capabilities for scene organization, traversal, and manipulation.

The implementation follows best practices for:
- **Performance**: O(1) lookups, efficient traversal algorithms
- **Safety**: Comprehensive error handling and validation
- **Usability**: High-level API with convenient utility functions
- **Maintainability**: Clean code structure with comprehensive documentation
- **Testability**: Extensive test coverage with multiple validation levels

The system is ready for production use and provides a solid foundation for advanced scene management features.
