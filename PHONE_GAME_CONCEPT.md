# Concept: "Doodle Dungeon" - turn hand-drawn floor plans into playable game maps

> A consumer mobile game built on IKore's **world-from-data** thesis
> (see `EXPANSION_IDEAS.md`). The player draws a floor plan on paper, snaps a
> photo, and seconds later is walking and playing inside their own drawing in 3D.
> The drawing is not just geometry - drawn symbols are a tiny visual language that
> places doors, enemies, loot, and the exit.

*(Working title only - other names: PaperWorlds, Sketch2World, Crayon Castle.)*

---

## 1. The magic moment

> "I scribbled a maze on a napkin, took a picture, and ten seconds later my kid
> was running through it being chased by slimes."

Everything in this concept serves that one 10-second loop:

**Draw -> Snap -> Play -> Share.**

The shareable artifact is unbeatable for social reach: a side-by-side of the
**paper drawing** and the **3D level it became** is exactly the kind of clip that
travels on TikTok/Instagram/YouTube Shorts. The product markets itself.

---

## 2. Why this is genuinely unique

- "Mario Maker for the real world" - user-generated levels, but the authoring tool
  is a **pencil and paper**, which everyone already has and enjoys.
- Almost no competitor does **photo-of-a-drawing -> 3D playable game**. (There are
  coloring-page AR apps and floor-plan *measurement* apps, but not a *game* loop.)
- Strong family / education angle: it rewards creativity and is safe, screen-light
  authoring (you draw on paper, not on a screen).
- It is the consumer face of a serious technology (the world-from-data pipeline),
  so the same core powers both a fun game and, later, "pro" use cases
  (architecture, sim, education).

---

## 3. The drawing *is* a language (the clever part)

The level is more than walls. A small set of easy-to-draw symbols becomes gameplay,
so a child can author a full level with a pencil:

| You draw... | It becomes... |
|---|---|
| Connected lines / closed shapes | Walls and rooms (extruded to 3D) |
| A gap in a wall | A doorway / passage |
| `S` (or a star) | Player start |
| `X` (or a flag) | Exit / goal |
| A small circle | Coin / collectible |
| A spiky blob | Enemy spawn |
| A wavy/zigzag line | Trap / hazard (lava, spikes) |
| `+` | Health / pickup |
| A square with a slash | Locked door (needs a key) |

This "symbol = object" mapping is the design heart: it keeps authoring effortless
*and* gives the game depth. The symbol set should start tiny (start, exit, enemy,
coin) and grow with the game's progression.

---

## 4. What you actually *do* - game loop

A map is not a game, so pick a core verb. Recommended first mode, in priority order:

1. **Top-down dungeon crawler** (twin-stick): explore your floor plan, grab coins,
   dodge/fight enemies, reach the exit. Easiest to make fun, reads well at any
   drawing quality, classic and broadly appealing. **<- start here.**
2. **First/third-person explorer**: "walk inside your drawing." Highest wow-factor
   for the share clip; great as a secondary "tour" mode.
3. **Stealth / hide-and-seek vs. AI agents**: leans on IKore's `AIComponent` +
   crowd simulation.
4. **Tower defense**: enemies path through your rooms - leans on the nav-mesh.
5. **Async multiplayer**: publish your map, friends play it with a share code;
   leaderboards for fastest clear. Leans on `NetworkComponent`. This is the
   long-term retention engine (endless UGC).

---

## 5. The hard core: hand-drawing -> playable map (the moat)

This computer-vision pipeline is the make-or-break and the defensible IP. Stages:

1. **Capture & rectify.** Detect the paper quad in the camera frame; perspective-
   warp to a clean top-down image (homography). Auto-crop, white-balance.
2. **Binarize & clean.** Grayscale -> adaptive threshold -> denoise -> deskew so a
   phone photo under any lighting becomes crisp black-on-white.
3. **Vectorize walls.** Detect line segments (e.g. Hough transform) and/or trace
   contours; merge collinear segments; **snap to grid and to 0/45/90° angles** so
   wobbly hand lines become clean architecture. Simplify with Douglas-Peucker.
4. **Build topology.** Close polygons into rooms; treat wall gaps as doorways;
   build a room-connectivity graph. Reject/repair unclosed or ambiguous regions.
5. **Recognize symbols.** Classify the drawn icons (start/exit/enemy/coin/...). Start
   with simple shape heuristics + template matching; graduate to a small on-device
   CNN trained on crowdsourced doodles as data accumulates. **This is where ML and
   the long-term data moat live** - every level players draw is training data.
6. **Emit the scene.** Produce IKore's intermediate scene description (see §7),
   which the engine extrudes to 3D, populates with entities, and bakes a nav-mesh
   from.

**De-risking order (critical):** do NOT start with messy camera input.
- **Phase 0:** on-screen finger/stylus drawing -> vectors are exact, no CV needed.
  Proves the *game* loop immediately.
- **Phase 1:** photo of a **clean, grid-paper, orthogonal** drawing.
- **Phase 2:** robust to freehand, lighting, angle, and color.
Each phase is shippable and teaches you what real drawings look like.

---

## 6. The honest tension: IKore is a *desktop* engine

IKore today is **C++ + desktop OpenGL** (GLFW, classic GL - verified: no OpenGL ES,
no EGL, no Android/iOS, no touch input, no OpenCV). It cannot ship to phones as-is.
There are three viable paths; the recommendation combines them:

| Path | What it means | Verdict |
|---|---|---|
| **A. Port IKore to mobile** | Add an OpenGL ES 3.0 / Vulkan backend, touch input, Android (NDK) + iOS build | Real work; do it *after* the loop is proven |
| **B. Extract the converter as a portable C++ library** | The IP is the *drawing->scene* pipeline, not the renderer. Ship it as a standalone lib usable from any mobile app/engine | **Smartest hedge** - value isn't locked to the renderer |
| **C. Prototype on desktop first** | Mouse-draw or load-photo -> extrude -> play, all in the engine you already have | **Start here** - proves the magic in days, not months |

**Recommendation:** **C -> B -> A.** Prove the loop on desktop with what already
exists; factor the drawing->scene converter into a clean library; only then invest
in the mobile renderer port (or pair the library with a mobile-native/Flutter/
Unity shell if that ships faster).

This keeps the bet cheap until the fun is proven, and ensures the core IP is
reusable no matter which renderer wins.

---

## 7. How it maps onto code that already exists

The desktop proof-of-concept (Phase 0) needs almost no new engine systems:

- **Intermediate scene format** -> reuse `SerializationData` / `JsonFormat`
  (`src/Serialization.h`). The converter outputs JSON; the engine loads it.
- **Walls/floors** -> `MeshComponent::createCube` / `createPlane`
  (`src/core/components/MeshComponent.h`) extruded along wall polylines.
- **Scene assembly** -> the existing scene graph (`SceneGraphSystem`) +
  entity/component system.
- **Collision & movement** -> `PhysicsComponent` (Bullet) for walls + a player body.
- **Enemies** -> `AIComponent` (patrol/chase already implemented).
- **Sound/feel** -> OpenAL `SoundComponent`; particles for coins/hits.
- **Share/multiplayer (later)** -> `NetworkComponent`.

The *new* code is the **drawing->scene converter** (§5) - which is also the
flagship importer from `EXPANSION_IDEAS.md`. The game and the engine roadmap are
the same project.

---

## 8. MVP - the smallest thing that proves the magic

**Desktop "Doodle Dungeon" PoC (Phase 0), buildable on current IKore:**

1. A simple draw surface (mouse) on a grid: draw wall segments + place 4 symbols
   (start, exit, coin, enemy). *(Or: hand-author a small JSON level to start even faster.)*
2. Converter: wall segments -> wall polygons -> 3D geometry; symbols -> entity spawns;
   write the JSON scene.
3. Load the scene; spawn a player with collision; top-down camera.
4. Core loop: move, collect coins, avoid one `AIComponent` enemy, reach the exit -> "You win."
5. A "tour mode" first-person camera for the wow-shot.

If that loop is fun with a *mouse-drawn* box-and-line map, the camera/CV and mobile
work are justified. If it isn't, you've spent days, not months.

---

## 9. Phased roadmap

| Phase | Scope | Outcome |
|---|---|---|
| **0. Desktop loop** | On-screen/JSON drawing -> 3D -> playable (MVP §8) | Proves the fun, reuses existing engine |
| **1. Clean-photo CV** | Photo of grid/orthogonal drawing -> vectors (§5 steps 1-4) | Proves "paper -> game" with constrained input |
| **2. Converter library** | Extract drawing->scene as portable C++ lib (Path B) | Reusable IP, testable in isolation |
| **3. Mobile shell** | OpenGL ES backend + touch, or mobile-native shell around the lib | Runs on a phone |
| **4. Symbol ML + robustness** | On-device symbol CNN; freehand/lighting robustness (§5 step 5) | Handles real kids' drawings |
| **5. UGC & social** | Share codes, level browser, leaderboards (`NetworkComponent`) | Retention + viral loop |

---

## 10. Top risks

1. **CV robustness on real drawings** - biggest risk. Mitigate with the
   Phase 0->2 ramp and grid-paper guidance; collect drawings as training data.
2. **Mobile port cost** - mitigate with Path B/C (don't port until proven).
3. **"Map ≠ game"** - mitigate by nailing one fun verb (top-down crawler) before
   adding modes.
4. **Scope creep** - the symbol set and game modes can balloon; ship the 4-symbol,
   one-mode MVP first.

---

## 11. Bottom line

The phone-game framing is the perfect consumer expression of the world-from-data
thesis: it makes the technology *delightful and shareable*, and the make-or-break
core (drawing -> 3D scene) is **the same importer the engine needs anyway**. Start
with the desktop PoC (§8) on the engine that already exists, keep the converter
renderer-agnostic, and only pay for mobile once the magic is proven.
