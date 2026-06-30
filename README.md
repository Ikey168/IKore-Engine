# IKore Engine

[![CI/CD Pipeline](https://github.com/Ikey168/IKore-Engine/actions/workflows/ci.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/ci.yml)
[![CodeQL](https://github.com/Ikey168/IKore-Engine/actions/workflows/analysis.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/analysis.yml)
[![Documentation](https://github.com/Ikey168/IKore-Engine/actions/workflows/docs.yml/badge.svg)](https://github.com/Ikey168/IKore-Engine/actions/workflows/docs.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**IKore Engine** is a modern 3D game engine written in C++17 and OpenGL, built
around an Entity-Component System with physics, audio, animation, and a forward
renderer with shadows and post-processing.

> **Where it's headed.** IKore is evolving from a general-purpose engine toward a
> focused identity: **building living 3D worlds from real data** - turning
> floor plans, maps, and sketches into navigable, simulated spaces. See
> [`EXPANSION_IDEAS.md`](EXPANSION_IDEAS.md) for the thesis and
> [`PHONE_GAME_CONCEPT.md`](PHONE_GAME_CONCEPT.md) /
> [`PHONE_GAME_DESIGN.md`](PHONE_GAME_DESIGN.md) for the flagship use case
> (a phone game that turns hand-drawn floor plans into playable 3D maps).

---

## Features

Everything below is implemented in the source tree today.

### Core architecture
- **Entity-Component System** with both a `shared_ptr`-based component model and a
  pooled variant (`EntityPool` / `PooledEntityManager`) for high entity counts.
- **Scene graph** with parent/child transforms.
- **Event system** for decoupled entity communication.
- **JSON serialization** of scenes and components.
- **Logging** and an **ECS micro-benchmark** harness.

### Components
`Transform` · `Mesh` (procedural cube/plane/sphere builders) · `Material` ·
`Renderable` · `Velocity` · `Physics` (Bullet) · `LOD` ·
`Animation` (skeletal, via Assimp) · `AI` (FSM: patrol / chase / flee / guard /
wander) · `Sound` / `Audio` (3D positional, OpenAL) · `Particle` · `Network`.

### Rendering
- Forward renderer (Phong/Blinn lighting).
- **Shadow mapping** - directional and point (cubemap) shadows.
- **Frustum culling**.
- **GPU/compute-shader particle system**.
- **Post-processing**: bloom, SSAO, FXAA.
- Skybox.

### Audio
- 3D positional audio with distance attenuation (OpenAL).

---

## Tech stack

- **Language:** C++17
- **Graphics:** OpenGL, [GLFW](https://github.com/glfw/glfw),
  [GLAD](https://github.com/Dav1dde/glad), [GLM](https://github.com/g-truc/glm)
- **Physics:** [Bullet3](https://github.com/bulletphysics/bullet3)
- **Audio:** OpenAL
- **Model/animation loading:** [Assimp](https://github.com/assimp/assimp) (OBJ, FBX)
- **Image loading:** [stb_image](https://github.com/nothings/stb)
- **Build:** CMake (dependencies fetched automatically via `FetchContent`)

---

## Building

IKore Engine uses CMake. GLFW, GLAD, GLM, stb_image, Assimp, and Bullet are
downloaded automatically at configure time via `FetchContent`. OpenAL is located
via your system package manager (`pkg-config`).

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

> **Linux audio prerequisite:** install OpenAL development headers, e.g.
> `sudo apt-get install libopenal-dev` (Debian/Ubuntu).

---

## Project structure

```
src/
  core/                Engine core: ECS, Entity, Transform, EventSystem, Logger
    components/         Component implementations (Transform, Mesh, Material, …)
  scene/               Scene graph & scene management
  audio/               OpenAL 3D audio engine and ambient zones
  shaders/             GLSL shaders (shadows, bloom, SSAO, FXAA, particles, …)
  demos/               Standalone demos
  tests/               Engine unit/component tests (built by CMake)
  main.cpp             Entry point / sample application
tests/                 Standalone test programs and shell test runners
assets/                Models, textures, and other runtime assets
docs/                  Per-subsystem implementation notes
```

Per-subsystem implementation notes live in [`docs/`](docs/). The project's
direction is documented in [`EXPANSION_IDEAS.md`](EXPANSION_IDEAS.md),
[`PHONE_GAME_CONCEPT.md`](PHONE_GAME_CONCEPT.md), and
[`PHONE_GAME_DESIGN.md`](PHONE_GAME_DESIGN.md).

---

## Roadmap & vision

IKore's near-term direction is to differentiate around **world-from-data**:

1. **Foundations** - a data-oriented (archetype) ECS, scripting + hot reload, and
   an in-engine editor (ImGui) that closes the open UI/debugging backlog.
2. **Flagship importer** - turn 2D/vector inputs (SVG floor plans, tilemaps, and
   later GeoJSON/OpenStreetMap) into 3D scenes with collision and navigation.
3. **Simulation & AI** - large-scale agent simulation with time control
   (pause / rewind / replay) and an AI-native authoring/NPC layer.

The full strategy and rationale are in:
- [`EXPANSION_IDEAS.md`](EXPANSION_IDEAS.md) - the engine's unique-identity thesis.
- [`PHONE_GAME_CONCEPT.md`](PHONE_GAME_CONCEPT.md) - the consumer flagship concept.
- [`PHONE_GAME_DESIGN.md`](PHONE_GAME_DESIGN.md) - deep design + market analysis.

---

## Contributing

Issues and pull requests are welcome. Development happens against feature branches;
CI (build, CodeQL analysis, and docs) runs on every push. Please keep new code
consistent with the surrounding style and add tests under `src/tests/` where it
makes sense.

---

## License

Released under the [MIT License](LICENSE). © 2025 Ikey168.
