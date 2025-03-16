# IKore Engine 🚀
*A lightweight, modular 3D game engine built with OpenGL*

## Overview
IKore Engine is a **modern, flexible, and extendable** 3D game engine designed for real-time rendering and game development. Powered by **OpenGL**, it provides essential features for building interactive 3D applications, including an **Entity-Component System (ECS)**, physics integration, and custom shaders.

## Features
✅ **OpenGL Rendering Pipeline** (VAO, VBO, Shaders)  
✅ **Entity-Component System (ECS)** for game objects  
✅ **Scene Graph for hierarchical transformations**  
✅ **First-person and third-person camera controls**  
✅ **Texture & Model Loading** (stb_image, Assimp)  
✅ **Basic Physics** (AABB collision, Bullet Physics integration)  
✅ **Keyboard & Mouse Input Handling (GLFW)**  
✅ **Post-Processing Effects** (Bloom, Motion Blur, FXAA)  
✅ **3D Positional Audio** (OpenAL)  

## Installation & Setup

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/IKoreEngine.git
cd IKoreEngine
```

### 2. Install Dependencies
Ensure you have the following dependencies installed:
- **GLFW** – Window & Input Handling
- **GLAD** – OpenGL Loader
- **GLM** – Mathematics Library
- **stb_image** – Texture Loading
- **Assimp** – Model Importing
- **Bullet Physics** – Physics Engine (Optional)

### 3. Build the Engine (Using CMake)
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./IKoreEngine
```

## Usage
Once the engine is running, you can:
- Load **3D models** and render them with OpenGL
- Move the **camera** using WASD + Mouse
- Enable **lighting, textures, and shading** effects
- Implement game logic using the **ECS architecture**

## Contributing
Contributions are welcome! If you’d like to contribute:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature-name`)
3. Commit changes and push to your fork
4. Open a pull request

## License
📜 **MIT License** – Free for personal & commercial use!  
