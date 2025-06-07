# IKore Engine

[![CI/CD Pipeline](https://github.com/Ikey168/IKore-Engine/actions/workflows/ci.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/ci.yml)
[![CodeQL](https://github.com/Ikey168/IKore-Engine/actions/workflows/analysis.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/analysis.yml)
[![Documentation](https://github.com/Ikey168/IKore-Engine/actions/workflows/docs.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/docs.yml)

IKore Engine is a modern, flexible 3D game engine built with OpenGL and C++.

## Building

IKore Engine uses CMake to manage its dependencies and generate native project files.

```bash
# From the repository root
mkdir build && cd build
cmake ..
cmake --build .
```

On Windows you can generate Visual Studio project files instead:

```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
```

On macOS you can generate Xcode projects:

```bash
mkdir build && cd build
cmake .. -G Xcode
```

GLFW, GLAD, GLM and stb_image are downloaded automatically at configure time via FetchContent and should work on Linux, macOS and Windows.
