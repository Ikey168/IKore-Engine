cmake_minimum_required(VERSION 3.10)

project(IKoreEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLAD
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v0.1.36
)
FetchContent_MakeAvailable(glad)

find_package(OpenGL REQUIRED)

add_executable(IKore src/main.cpp)

target_link_libraries(IKore PRIVATE glfw glad OpenGL::GL)

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
