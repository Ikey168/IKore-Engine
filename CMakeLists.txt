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
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
  add_library(stb_image INTERFACE)
  target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})
endif()

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

find_package(OpenGL REQUIRED)

add_executable(IKore src/main.cpp src/core/Logger.cpp src/DeltaTimeDemo.cpp src/Shader.cpp src/Texture.cpp src/Model.cpp src/PostProcessor.cpp src/Camera.cpp src/CameraController.cpp src/Skybox.cpp src/ParticleSystem.cpp src/ShadowMap.cpp src/Frustum.cpp src/core/Entity.cpp src/core/EntityTypes.cpp src/core/Transform.cpp src/core/TransformableEntities.cpp src/InputComponent.cpp src/ControllableEntities.cpp src/CameraComponent.cpp src/EnhancedCameraEntity.cpp src/Serialization.cpp src/core/EntityDebugSystem.cpp src/scene/SceneGraphSystem.cpp src/scene/SceneManager.cpp)

# Scene Graph Test executable
add_executable(test_scene_graph src/tests/test_scene_graph.cpp src/core/Logger.cpp src/core/Entity.cpp src/core/EntityTypes.cpp src/core/Transform.cpp src/core/TransformableEntities.cpp src/scene/SceneGraphSystem.cpp src/scene/SceneManager.cpp)

# Scene Graph Demo executable
add_executable(scene_graph_demo src/demos/scene_graph_demo.cpp src/core/Logger.cpp src/core/Entity.cpp src/core/EntityTypes.cpp src/core/Transform.cpp src/core/TransformableEntities.cpp src/scene/SceneGraphSystem.cpp src/scene/SceneManager.cpp)

# Simple Scene Graph Test (minimal)
add_executable(simple_scene_test src/tests/simple_scene_test.cpp src/core/Entity.cpp src/core/EntityTypes.cpp src/core/Transform.cpp src/core/TransformableEntities.cpp)

# Logger Test (minimal)
add_executable(logger_test src/tests/logger_test.cpp src/core/Logger.cpp)

# Filesystem Test (debug)
add_executable(filesystem_test src/tests/filesystem_test.cpp)

# Debug Logger Test
add_executable(debug_logger_test src/tests/debug_logger_test.cpp)

target_link_libraries(IKore PRIVATE glfw glad glm stb_image assimp OpenGL::GL)
target_link_libraries(test_scene_graph PRIVATE glm)
target_link_libraries(scene_graph_demo PRIVATE glm)
target_link_libraries(simple_scene_test PRIVATE glm)
target_link_libraries(logger_test PRIVATE)
target_link_libraries(filesystem_test PRIVATE)
target_link_libraries(debug_logger_test PRIVATE)

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
