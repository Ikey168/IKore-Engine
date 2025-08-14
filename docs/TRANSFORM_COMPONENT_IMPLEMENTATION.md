# Transform Component Implementation Guide

## Overview

This document provides a comprehensive guide to the Transform Component system implemented for the IKore Engine as part of Issue #24. The Transform Component provides a robust, hierarchical 3D transformation system that integrates seamlessly with the Entity System.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Classes](#core-classes)
3. [Features](#features)
4. [Usage Examples](#usage-examples)
5. [Integration with Entity System](#integration-with-entity-system)
6. [Interactive Controls](#interactive-controls)
7. [Technical Details](#technical-details)
8. [Performance Considerations](#performance-considerations)
9. [Testing](#testing)

## Architecture Overview

The Transform Component system consists of several key components:

```
Transform System Architecture:
├── Transform (Core transformation class)
├── TransformComponent (Entity integration wrapper)
├── TransformableGameObject (Enhanced GameObject with transforms)
├── TransformableLight (Enhanced Light with transforms)
├── TransformableCamera (Enhanced Camera with transforms)
└── Entity System Integration
```

## Core Classes

### 1. Transform Class (`src/Transform.h/cpp`)

The `Transform` class is the core of the transformation system, providing:

#### Key Features:
- **Position**: 3D position storage and manipulation
- **Rotation**: Euler angle rotation (degrees) with quaternion support
- **Scale**: 3D scale values with uniform and non-uniform scaling
- **Hierarchy**: Parent-child relationships with automatic propagation
- **Matrix Generation**: Local and world transformation matrices
- **Performance**: Lazy evaluation with dirty flag system

#### Core Methods:

```cpp
// Position manipulation
void setPosition(const glm::vec3& position);
void translate(const glm::vec3& translation);
glm::vec3 getWorldPosition() const;

// Rotation manipulation  
void setRotation(const glm::vec3& rotation);
void rotate(const glm::vec3& rotation);
glm::quat getRotationQuaternion() const;

// Scale manipulation
void setScale(const glm::vec3& scale);
void scale(const glm::vec3& scaleFactor);
glm::vec3 getWorldScale() const;

// Matrix access
const glm::mat4& getLocalMatrix() const;
glm::mat4 getWorldMatrix() const;

// Hierarchy management
void setParent(Transform* parent);
void addChild(Transform* child);
```

### 2. TransformComponent Class

Wrapper class that integrates Transform with the Entity System:

```cpp
class TransformComponent {
public:
    TransformComponent();
    TransformComponent(const glm::vec3& position, 
                      const glm::vec3& rotation = glm::vec3(0.0f), 
                      const glm::vec3& scale = glm::vec3(1.0f));
    
    Transform& getTransform() { return m_transform; }
    const Transform& getTransform() const { return m_transform; }
    
    void update(float deltaTime);
    void initialize();
    void cleanup();
};
```

### 3. Enhanced Entity Types

#### TransformableGameObject
Enhanced GameObject with full transform support and hierarchy management:

```cpp
class TransformableGameObject : public Entity {
public:
    TransformableGameObject(const std::string& name, 
                           const glm::vec3& position = glm::vec3(0.0f),
                           const glm::vec3& rotation = glm::vec3(0.0f),
                           const glm::vec3& scale = glm::vec3(1.0f));
    
    // Transform access
    Transform& getTransform() { return m_transformComponent.getTransform(); }
    
    // Hierarchy management (type-safe)
    void setParent(TransformableGameObject* parent);
    void addChild(std::shared_ptr<TransformableGameObject> child);
    
    // Rendering support
    glm::mat4 getWorldMatrix() const;
    bool isVisible() const { return m_visible; }
};
```

#### TransformableLight
Light entity with transform support for 3D positioning and orientation:

```cpp
class TransformableLight : public Entity {
public:
    enum class LightType { DIRECTIONAL, POINT, SPOT, AREA };
    
    TransformableLight(const std::string& name,
                      LightType type = LightType::POINT,
                      const glm::vec3& position = glm::vec3(0.0f),
                      const glm::vec3& color = glm::vec3(1.0f),
                      float intensity = 1.0f);
    
    // Light-specific transform methods
    glm::vec3 getDirection() const;          // For directional/spot lights
    void setDirection(const glm::vec3& direction);
    glm::vec3 getWorldPosition() const;
};
```

#### TransformableCamera
Camera entity with transform support for 3D positioning and view matrix generation:

```cpp
class TransformableCamera : public Entity {
public:
    enum class ProjectionType { PERSPECTIVE, ORTHOGRAPHIC };
    
    TransformableCamera(const std::string& name,
                       const glm::vec3& position = glm::vec3(0.0f, 0.0f, 5.0f),
                       const glm::vec3& rotation = glm::vec3(0.0f),
                       float fov = 45.0f,
                       float aspectRatio = 16.0f / 9.0f,
                       float nearPlane = 0.1f,
                       float farPlane = 100.0f);
    
    // Camera-specific methods
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjectionMatrix() const;
    void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
};
```

## Features

### 1. Hierarchical Transformations

The Transform system supports parent-child relationships where child transforms are relative to their parent:

```cpp
// Create hierarchy
auto parent = entityManager.createEntity<TransformableGameObject>("Parent");
auto child = entityManager.createEntity<TransformableGameObject>("Child");

// Set up hierarchy
child->setParent(parent.get());
child->getTransform().setPosition(0.0f, 2.0f, 0.0f); // Local position

// Child's world position is automatically calculated relative to parent
glm::vec3 worldPos = child->getTransform().getWorldPosition();
```

### 2. Matrix Caching and Performance

The system uses lazy evaluation with dirty flags to optimize matrix calculations:

```cpp
// Matrices are only recalculated when needed
transform.setPosition(1.0f, 2.0f, 3.0f);  // Marks as dirty
transform.setRotation(45.0f, 0.0f, 0.0f); // Still dirty
glm::mat4 matrix = transform.getLocalMatrix(); // Now calculates matrix
```

### 3. Multiple Coordinate Systems

Support for both local and world coordinate transformations:

```cpp
// Local coordinates (relative to parent)
transform.setPosition(1.0f, 0.0f, 0.0f);
glm::vec3 localPos = transform.getPosition();

// World coordinates (absolute in world space)
glm::vec3 worldPos = transform.getWorldPosition();
```

### 4. Direction Vectors

Easy access to orientation-based direction vectors:

```cpp
// Get direction vectors in world space
glm::vec3 forward = transform.getForward();  // -Z axis
glm::vec3 right = transform.getRight();      // +X axis  
glm::vec3 up = transform.getUp();            // +Y axis

// Look at functionality
transform.lookAt(targetPosition);
```

## Usage Examples

### Basic Transform Usage

```cpp
// Create a transformable game object
auto gameObject = entityManager.createEntity<TransformableGameObject>("MyObject");

// Set initial transform
gameObject->getTransform().setPosition(5.0f, 0.0f, 0.0f);
gameObject->getTransform().setRotation(0.0f, 45.0f, 0.0f);
gameObject->getTransform().setScale(2.0f);

// Animate the object
gameObject->getTransform().rotate(0.0f, 1.0f, 0.0f); // Rotate 1 degree per frame
gameObject->getTransform().translate(0.1f, 0.0f, 0.0f); // Move along X
```

### Creating Object Hierarchies

```cpp
// Create parent object
auto vehicle = entityManager.createEntity<TransformableGameObject>("Vehicle");
vehicle->getTransform().setPosition(10.0f, 0.0f, 0.0f);

// Create child objects
auto wheel1 = entityManager.createEntity<TransformableGameObject>("Front Wheel");
wheel1->getTransform().setPosition(2.0f, -1.0f, 1.0f); // Local to vehicle
wheel1->setParent(vehicle.get());

auto wheel2 = entityManager.createEntity<TransformableGameObject>("Rear Wheel");
wheel2->getTransform().setPosition(-2.0f, -1.0f, 1.0f); // Local to vehicle
wheel2->setParent(vehicle.get());

// Moving the vehicle moves all wheels automatically
vehicle->getTransform().translate(1.0f, 0.0f, 0.0f);
```

### Light Positioning

```cpp
// Create a point light
auto light = entityManager.createEntity<TransformableLight>(
    "Room Light",
    TransformableLight::LightType::POINT,
    glm::vec3(0.0f, 3.0f, 0.0f), // Position
    glm::vec3(1.0f, 0.8f, 0.6f),  // Warm white color
    2.0f  // Intensity
);

// Animate the light
light->getTransform().translate(0.0f, 0.0f, 0.1f * sin(time));
```

### Camera Control

```cpp
// Create a camera
auto camera = entityManager.createEntity<TransformableCamera>(
    "Main Camera",
    glm::vec3(0.0f, 5.0f, 10.0f), // Position
    glm::vec3(-30.0f, 0.0f, 0.0f)  // Look down slightly
);

// Make camera follow an object
camera->lookAt(targetObject->getTransform().getWorldPosition());

// Get matrices for rendering
glm::mat4 viewMatrix = camera->getViewMatrix();
glm::mat4 projMatrix = camera->getProjectionMatrix();
```

## Integration with Entity System

The Transform Component integrates seamlessly with the existing Entity System:

### Entity Updates

```cpp
// Transform components are automatically updated with entities
entityManager.updateAll(deltaTime); // Updates all transform components
```

### Type-Safe Entity Management

```cpp
// Create and manage transformable entities
auto entities = entityManager.getAllEntities();
for (auto entity : entities) {
    // Check if entity has transform capability
    auto transformable = std::dynamic_pointer_cast<TransformableGameObject>(entity);
    if (transformable) {
        // Access transform safely
        transformable->getTransform().update(deltaTime);
    }
}
```

## Interactive Controls

The implementation includes interactive controls for testing and demonstration:

| Key | Action |
|-----|--------|
| **Y** | Rotate parent object |
| **U** | Scale parent object |
| **I** | Move transformable light |
| **O** | Move transformable camera |
| **P** | Print transform hierarchy information |

### Control Implementation

```cpp
// Example control handler
else if (key == GLFW_KEY_Y) {
    // Rotate parent object
    entityManager.forEach([](std::shared_ptr<Entity> entity) {
        auto transformableObj = std::dynamic_pointer_cast<TransformableGameObject>(entity);
        if (transformableObj && transformableObj->getName() == "Parent Object") {
            transformableObj->getTransform().rotate(0.0f, 15.0f, 0.0f);
        }
    });
}
```

## Technical Details

### Thread Safety

- Transform operations are designed to be called from the main thread
- EntityManager provides thread-safe access to transform entities
- Matrix calculations are performed on-demand to avoid race conditions

### Memory Management

- Uses RAII principles with smart pointers
- Automatic cleanup of parent-child relationships
- Efficient memory usage with shared entity ownership

### Matrix Mathematics

- Uses GLM library for all mathematical operations
- Transformation order: Scale → Rotation → Translation
- Euler angles stored in degrees for user convenience
- Quaternion support for advanced rotation operations

### Coordinate System

- Right-handed coordinate system
- Y-up orientation
- Forward direction is -Z axis
- Rotation order: Y (Yaw) → X (Pitch) → Z (Roll)

## Performance Considerations

### Optimization Features

1. **Lazy Matrix Evaluation**: Matrices are only calculated when accessed
2. **Dirty Flag System**: Avoids unnecessary recalculations
3. **Hierarchy Propagation**: Efficient parent-child update propagation
4. **Memory Pooling**: Efficient memory usage with smart pointers

### Performance Tips

```cpp
// Batch transform operations
transform.setPosition(newPos);
transform.setRotation(newRot);
transform.setScale(newScale);
// Matrix calculated only once when accessed

// Use world positions sparingly in tight loops
if (transform.isDirty()) {
    glm::vec3 worldPos = transform.getWorldPosition(); // Expensive
    // Cache and reuse worldPos
}
```

## Testing

### Automated Testing

Run the automated test script:

```bash
# Run comprehensive tests
./test_transform_component.sh
```

The test script verifies:
- ✅ File structure and includes
- ✅ Class definitions and interfaces
- ✅ Build system integration
- ✅ Compilation success
- ✅ Feature completeness
- ✅ Acceptance criteria fulfillment

### Manual Testing

1. **Build and run the application**:
   ```bash
   cd build
   ./IKore
   ```

2. **Test interactive controls**:
   - Press Y to rotate parent object
   - Press U to scale parent object
   - Press I to move light
   - Press O to move camera
   - Press P to print hierarchy

3. **Verify console output**:
   - Transform creation messages
   - Hierarchy information
   - Position/rotation updates

### Acceptance Criteria Verification

✅ **Entities can store and update transformations**
- Transform class stores position, rotation, scale
- TransformComponent integrates with Entity System
- Update methods process transformations each frame

✅ **Parent-child transformations work correctly**
- Hierarchical relationships with setParent/addChild
- World matrix calculation considers parent transforms
- Children move relative to parents automatically

✅ **Changes reflect in rendering**
- getWorldMatrix() provides rendering transformation
- Matrix recalculation when transforms change
- Integration with rendering pipeline

## Conclusion

The Transform Component system provides a comprehensive, efficient, and user-friendly solution for 3D transformations in the IKore Engine. It successfully meets all acceptance criteria and provides a solid foundation for game object positioning, hierarchical animations, lighting systems, and camera controls.

The implementation emphasizes:
- **Performance**: Lazy evaluation and dirty flag optimization
- **Usability**: Intuitive API with comprehensive features
- **Integration**: Seamless Entity System compatibility
- **Extensibility**: Easy to extend for specialized use cases
- **Robustness**: Memory safety and error handling

This Transform Component system is ready for production use and provides all necessary features for a modern 3D game engine.
