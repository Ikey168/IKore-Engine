# IKore Engine — Expansion Ideas: Finding a Unique Identity

> **Purpose of this document.** IKore Engine is technically competent, but in its
> current form it is hard to distinguish from the hundreds of other hobby
> OpenGL/C++ ECS engines on GitHub. This document proposes a *point of view* —
> a niche where IKore would be the obvious choice — and a concrete, phased plan
> to get there, grounded in the code that already exists.

---

## 1. Honest assessment of where the engine is today

**What is actually implemented in `src/` (verified in code):**

- A working **Entity-Component System** in two flavors: a `shared_ptr`-per-component
  OOP style (`Entity::addComponent<T>()`) and a pooled variant
  (`EntityPool`, `PooledEntityManager`).
- Components: `Transform`, `Mesh`, `Material`, `Renderable`, `Velocity`,
  `Physics` (Bullet), `LOD`, `Animation` (Assimp skeletal), `AI` (FSM:
  patrol/chase/flee/guard/wander), `Sound`/`Audio` (OpenAL 3D), `Particle`,
  `Network`.
- Rendering: forward renderer with **shadow mapping** (directional + point),
  **frustum culling**, a **GPU/compute particle system**, post-processing
  (**bloom, SSAO, FXAA**), and a skybox.
- Engine services: **scene graph**, **serialization**, **event system**,
  logger, and an **ECS micro-benchmark**.

**What the issue tracker *claims* but is NOT in the source tree:**

- Terrain (deformation, blending, heightmaps), water/buoyancy, weather physics,
  destructibles, and the whole **"map translation" milestone**
  (SVG→3D, procedural roads, OSM-style city generation, randomized object
  variation). These issues are marked *closed*, but `grep` finds **no surviving
  code** for terrain, water, weather, SVG, or heightmaps. They were planned and
  abandoned (or reverted).

**What is entirely missing:**

- **No editor / in-engine UI / tooling.** Every issue in "Milestone 7: UI &
  Debugging" (#53–#63: FPS overlay, debug console, entity inspector, scene
  hierarchy viewer, input remapping, HUD/menus) is still **open**.
- **No scripting layer** (no Lua / AngelScript / WASM). Everything is hard-coded C++.
- **No PBR / modern rendering path** (it is classic Phong/Blinn forward shading).

### The core problem

> IKore is a **general-purpose engine with no reason to exist** next to Godot,
> Unreal, Bevy, or the next hobby engine. Every feature it has is a feature those
> engines have, done better. To become interesting it needs a *thesis* — one
> thing it does that mainstream engines do **not**.

The good news: the **most distinctive idea in the project's own history — turning
real-world / 2D data into living 3D worlds — was never actually built.** That is
the gap to lean into.

---

## 2. The thesis: *"The engine that builds living worlds from real data."*

Instead of competing as a general game engine, position IKore as a
**world-from-data simulation engine**: feed it 2D/real-world inputs (floor plans,
city maps, GIS data, tilemaps), and it produces a navigable, physically simulated
3D world populated by autonomous agents — with the ability to pause, rewind,
replay, and export the simulation.

This is a real, under-served niche (architectural walkthroughs, urban/traffic
simulation, crowd modeling, digital twins, rapid game-level prototyping from real
maps) and it is *coherent with the engine's own abandoned roadmap*. Three pillars
make it unique, and each one builds directly on code that already exists:

| Pillar | Builds on existing code | Why it's unique |
|---|---|---|
| **A. World-from-Data pipeline** | scene graph, `MeshComponent` procedural builders, serialization | Few hobby engines ingest SVG/GeoJSON/CAD and extrude playable 3D worlds |
| **B. Agent simulation at scale + time control** | `AIComponent`, `EventSystem`, Bullet physics, `EntityPool` | Pause/rewind/replay of thousands of deterministic agents is rare |
| **C. AI-native authoring & NPCs** | `AIComponent` (FSM), `EventSystem`, `NetworkComponent` | LLM-assisted scene authoring + LLM/behavior-tree NPCs is genuinely 2026-modern |

The rest of the document details these three flagship/supporting directions, plus
the **foundations** they require, and ends with a concrete 8-week vertical slice.

---

## 3. Tier 1 — Flagship differentiator: the World-from-Data pipeline

**Goal:** `ikore import city.osm` (or `floorplan.svg`, or `level.tmx`) → a fully
navigable 3D scene with walls, roads, props, collision, and a baked nav-mesh.

### Phases

1. **SVG / vector floor plans → 3D interiors.**
   Parse SVG paths; extrude walls to height; cut doors/windows from layer
   metadata; generate floor/ceiling geometry. Output entities through the
   existing scene graph + `MeshComponent`. (This was issue #71, never built.)
2. **GeoJSON / OpenStreetMap → city blocks.**
   Buildings from footprint polygons (extrude by `height`/`levels` tags), roads
   from way geometry with curve smoothing and intersection stitching (issue #72),
   water/parks as flat textured regions.
3. **Tilemaps (Tiled `.tmx`) → 3D levels.**
   Map tile layers to prefabs; object layers to entity spawns. The fastest path
   to a usable demo and the friendliest to game devs.
4. **Procedural dressing.**
   Randomized rotation/scale/placement of props with overlap avoidance
   (issue #73); biome/material rules per region.
5. **Navigation baking.**
   Generate a nav-mesh from the produced geometry so agents (Pillar B) can move.

### Why this is the right flagship
- It is the project's *own* original differentiator, currently unbuilt.
- It immediately produces impressive, shareable demos ("I imported my city and
  walked through it") — great for stars/visibility.
- It reuses the scene graph, procedural mesh builders, and serialization that
  already work.
- It creates a reason to choose IKore over Godot/Unreal for a whole class of
  users (GIS, architecture, simulation, viz).

### Rough effort
SVG interiors: ~2–3 weeks. Tilemaps: ~1 week. GeoJSON/OSM: ~3–4 weeks.
Nav-mesh: ~2 weeks (or integrate Recast/Detour).

---

## 4. Tier 2 — Supporting differentiators

### 4A. Agent simulation at scale, with time control
Turn the FSM `AIComponent` into a **simulation substrate**:
- **Deterministic fixed-step update** so runs are reproducible.
- **Time control**: pause, step, variable speed, and **rewind/replay** via
  ring-buffered state snapshots (the serialization system is the seed).
- **Scale**: thousands of agents (crowds, traffic, ecosystems). This *requires*
  the data-oriented ECS in §5.1 — the current `shared_ptr`-per-component model
  will not hold thousands of agents at frame rate.
- **Data export**: dump per-tick agent state to CSV/JSON for analysis.

*Unique because* mainstream game engines optimize for a few hero characters, not
reproducible, rewindable simulation of large populations.

### 4B. AI-native authoring and NPCs (the 2026 hook)
- **Natural-language scene authoring**: "add a market square with 8 stalls and a
  fountain" → the engine places entities via the World-from-Data primitives.
- **LLM-driven NPCs**: replace/augment the FSM with goal-oriented agents that
  have memory and emit/consume `EventSystem` events; behavior-tree fallback when
  offline. Use the latest Claude models (e.g. `claude-opus-4-8`) behind a thin,
  swappable `IBrain` interface so the engine never hard-depends on one provider.
- **Procedural quests/dialogue** generated from world state.

*Unique because* very few open C++ engines ship an AI-native authoring + NPC
layer, and IKore already has the `AIComponent`/`EventSystem`/`NetworkComponent`
scaffolding to host it.

### 4C. Deterministic replay & rollback netcode
Extend `NetworkComponent` toward **deterministic lockstep + rollback**
(GGPO-style). Pairs naturally with 4A's deterministic core and gives credible
multiplayer *and* reproducible simulations.

*Unique because* rollback netcode is rare in indie/hobby engines and is a strong
draw for both competitive games and distributed simulation.

---

## 5. Tier 3 — Foundations that unlock the above

These are not unique on their own, but the flagship cannot ship without them.

### 5.1 Data-oriented (archetype) ECS  *(highest-leverage foundation)*
The current ECS stores each component behind a `shared_ptr` on the entity —
pointer-chasing, cache-unfriendly, and a hard ceiling on agent counts. Move to an
**archetype/SoA storage** with system queries iterating contiguous arrays.
The existing `ECSBenchmark` can prove the before/after numbers. This single change
is what makes "thousands of agents" (4A) and large imported cities (Tier 1)
viable.

### 5.2 Scripting + hot reload
Embed **Lua** (or AngelScript) and expose components/events. Add **hot reload**
of scripts, shaders, and assets. This turns IKore from "recompile to change
anything" into a live creator loop, and lets non-C++ users drive the
World-from-Data pipeline.

### 5.3 In-engine editor (closes the open backlog)
Integrate **Dear ImGui** and knock out the entire stalled Milestone 7 in one
coherent push:
- FPS / perf overlay (#53), debug console with commands (#54), entity inspector
  with live edit (#56), scene hierarchy viewer (#59), input remapping (#60),
  HUD/menus (#55, #58), benchmarking UI (#62).
An editor is table-stakes for anyone to *use* the world-building pipeline.

### 5.4 Rendering modernization *(optional, "catch-up" not "unique")*
PBR materials + IBL, a clustered/deferred path, and basic GI. Worthwhile for
visual credibility of imported worlds, but it does not differentiate on its own —
prioritize it below Tiers 1–2.

---

## 6. Concrete first step: an 8-week "vertical slice" that proves the identity

Ship one end-to-end demo that no generic engine ships out of the box:

> **"Import a real neighborhood from OpenStreetMap, walk through it, and watch
> 500 autonomous pedestrians navigate it — then rewind time."**

| Weeks | Deliverable |
|---|---|
| 1–2 | Archetype ECS conversion of the hot path (§5.1); validate with `ECSBenchmark` |
| 2–3 | Tilemap **or** SVG importer → scene graph entities (§3, smallest viable) |
| 4–5 | GeoJSON/OSM building+road import; nav-mesh bake (§3) |
| 5–6 | Deterministic fixed-step + 500 crowd agents on the nav-mesh (§4A) |
| 6–7 | ImGui overlay: FPS, entity inspector, **rewind/replay** scrubber (§5.3, §4A) |
| 7–8 | Polish, record a demo GIF, write a tutorial in the README |

That single demo is the elevator pitch the project currently lacks.

---

## 7. Quick wins (do these regardless of direction)

These are low-effort, high-signal improvements that make the repo credible enough
for any of the above to attract contributors:

1. **Repo hygiene.** The root holds ~30 one-off `fix_*.sh`, `test_*.sh`, and
   `*_IMPLEMENTATION.md` files plus committed build dirs (`build_ci/`,
   `build_test/`) and binaries (`test_ai_component`, `test_openal_3d_audio_fallback`).
   Move docs to `docs/`, tests to `tests/`, delete committed build artifacts, and
   add them to `.gitignore`. This alone makes the project look maintained.
2. **One real README.** Replace the build-only README with: what the engine is,
   the thesis (§2), a screenshot/GIF, and a feature matrix of what truly works.
3. **Reconcile the issue tracker with reality.** Re-open (or honestly close with a
   note) the terrain/water/weather/map issues whose code is not in the tree, so
   the backlog reflects the actual state.
4. **A `samples/` folder** with 2–3 runnable scenes, so a newcomer sees results in
   one command.
5. **FPS + entity-count overlay (#53, ImGui).** Smallest possible step toward the
   editor, and instantly useful.

---

## 8. Summary — pick a lane

If IKore tries to be a better general-purpose engine, it loses. If it becomes
**the engine that turns real-world and 2D data into living, rewindable, AI-driven
3D simulations**, it occupies a niche the big engines ignore — and it does so by
*finishing the vision it already started* and *building on the systems it already
has*.

**Recommended order:** §5.1 archetype ECS → §3 one importer → §4A crowd + time
control → §5.3 ImGui editor → then §4B AI-native layer and §4C netcode as the
project matures. Start with the §6 vertical slice.
