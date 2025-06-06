# IKore Engine
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

This setup downloads GLFW, GLAD, GLM and stb_image at configure time and should work on Linux, macOS and Windows.
