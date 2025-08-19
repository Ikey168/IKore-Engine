cmake_minimum_required(VERSION 3.10)

project(IKoreEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLAD
set(GLAD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v0.1.36
)
FetchContent_MakeAvailable(glad)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

# stb_image
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        f58f558c120e9b32c217290b80bad1a0729fbb2c
)
FetchContent_MakeAvailable(stb)
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# Assimp
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG        v5.3.1
)
FetchContent_MakeAvailable(assimp)

# Bullet Physics
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(INSTALL_LIBS ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  bullet3
  GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
  GIT_TAG        3.25
)
FetchContent_MakeAvailable(bullet3)

# OpenAL for 3D Audio
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(OPENAL openal)
endif()

if(NOT OPENAL_FOUND)
    # Try to find OpenAL manually
    find_path(OPENAL_INCLUDE_DIR AL/al.h
        HINTS
        /usr/include
        /usr/local/include
        /opt/local/include
    )
    
    find_library(OPENAL_LIBRARY
        NAMES openal OpenAL32
        HINTS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    )
    
    if(OPENAL_INCLUDE_DIR AND OPENAL_LIBRARY)
        set(OPENAL_FOUND TRUE)
        set(OPENAL_LIBRARIES ${OPENAL_LIBRARY})
        set(OPENAL_INCLUDE_DIRS ${OPENAL_INCLUDE_DIR})
    endif()
endif()

if(OPENAL_FOUND)
    message(STATUS "Found OpenAL: ${OPENAL_LIBRARIES}")
    # Set preprocessor definition for conditional compilation
    add_definitions(-DOPENAL_FOUND=1)
else()
    message(WARNING "OpenAL not found. Audio system will be compiled in fallback mode without OpenAL libraries.")
    # Create dummy variables for compilation
    set(OPENAL_LIBRARIES "")
    set(OPENAL_INCLUDE_DIRS "")
    add_definitions(-DOPENAL_FOUND=0)
endif()

find_package(OpenGL REQUIRED)

# Add include directories for the new folder structure
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add Bullet Physics include directories
include_directories(${bullet3_SOURCE_DIR}/src)

# Add OpenAL include directories
if(OPENAL_FOUND)
    include_directories(${OPENAL_INCLUDE_DIRS})
endif()

# Define source files explicitly
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EntityTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TransformableEntities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EntityDebugSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/MeshComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/MaterialComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/PhysicsComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AudioComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AIComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AnimationComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/NetworkComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/AISystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/AnimationLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/NetworkSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ECSBenchmark.cpp
)

set(AUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/OpenALAudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioSystem3D.cpp
)

set(SCENE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/SceneGraphSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/SceneManager.cpp
)

set(ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeltaTimeDemo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CameraController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Skybox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ParticleSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InputComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ControllableEntities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CameraComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EnhancedCameraEntity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Serialization.cpp
)

add_executable(IKore ${ENGINE_SOURCES} ${CORE_SOURCES} ${SCENE_SOURCES} ${AUDIO_SOURCES})

# Scene Graph Test executable
add_executable(test_scene_graph 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_scene_graph.cpp 
    ${CORE_SOURCES}
    ${SCENE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# Scene Graph Demo executable
add_executable(scene_graph_demo 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/demos/scene_graph_demo.cpp 
    ${CORE_SOURCES}
    ${SCENE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# MeshComponent Test executable
add_executable(test_mesh_component 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_mesh_component.cpp 
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# MeshComponent Minimal Test executable  
add_executable(test_mesh_component_minimal 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_mesh_component_minimal.cpp 
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# MeshComponent Geometry Test executable  
add_executable(test_mesh_component_geometry 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_mesh_component_geometry.cpp 
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# MaterialComponent Test executable
add_executable(test_material_component 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_material_component.cpp 
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frustum.cpp
)

# PhysicsComponent Test executable (simplified)
add_executable(test_physics_component 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_component.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
)

# PhysicsComponent Simple Test executable
add_executable(test_physics_simple 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_simple.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/PhysicsComponent.cpp
)

# ECS Benchmark Test executable (simplified)
add_executable(test_ecs_benchmark 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ecs_benchmark.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
)

# OpenAL 3D Audio Test executable
add_executable(test_openal_3d_audio 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_openal_3d_audio.cpp 
    # Minimal core sources needed for audio testing (excluding graphics dependencies)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EntityTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TransformableEntities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EntityDebugSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AudioComponent.cpp
    ${AUDIO_SOURCES}
)

# Note: Placeholder test files removed from build until implemented
# These were causing linker errors due to missing main() functions:
# - simple_scene_test.cpp
# - logger_test.cpp 
# - filesystem_test.cpp
# - debug_logger_test.cpp

target_link_libraries(IKore PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_scene_graph PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(scene_graph_demo PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_mesh_component PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_mesh_component_minimal PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_mesh_component_geometry PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_material_component PRIVATE glfw glad glm stb_image assimp OpenGL::GL BulletDynamics BulletCollision LinearMath ${OPENAL_LIBRARIES})
target_link_libraries(test_physics_component PRIVATE glm)
target_link_libraries(test_physics_simple PRIVATE BulletDynamics BulletCollision LinearMath glm)
target_link_libraries(test_ecs_benchmark PRIVATE glm)
target_link_libraries(test_openal_3d_audio PRIVATE glm ${OPENAL_LIBRARIES})

# AI Component Test executable
add_executable(test_ai_component 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ai_component.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AIComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/AISystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
)

target_link_libraries(test_ai_component PRIVATE glm)

# AnimationComponent test
add_executable(test_animation_component 
    test_animation_component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/AnimationComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/AnimationLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
)
target_link_libraries(test_animation_component PRIVATE glad glfw glm::glm assimp)

# NetworkComponent test
add_executable(test_network_component 
    test_network_component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/components/NetworkComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/NetworkSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Entity.cpp
)
target_link_libraries(test_network_component PRIVATE glm::glm)

if(MSVC)
    target_compile_options(IKore PRIVATE /W4)
else()
    target_compile_options(IKore PRIVATE -Wall -Wextra -pedantic)
endif()

# Multi-platform options
if (APPLE)
    target_link_libraries(IKore PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

install(TARGETS IKore RUNTIME DESTINATION bin)
