# Rendering Modernization Plan

Status: proposed. This is the "short plan of prioritized improvements" that issue
#226 asks to agree on **before** any large rendering change. It is optional and
"catch-up, not unique" per `EXPANSION_IDEAS.md` section 5.4: rendering is not the
engine's differentiator, so the guiding principle is **high value, low risk,
isolated, and no regressions** to existing scenes.

The engine today has a forward renderer with Blinn-Phong lighting, directional and
point (cubemap) shadow maps, frustum culling, a GPU/compute particle system, and
post-processing (bloom, SSAO, FXAA) plus a skybox.

## Guiding constraints

- Each change is isolated and independently revertible.
- Prefer additive, data-driven changes over renderer rewrites.
- Anything testable gets a headless unit test (the pass graph below is the first).
- No visual regression to existing sample scenes.

## Prioritized improvements

### P1 - Render-pass graph / pass abstraction (foundation + adoption, done)

Describe the frame as named passes with explicit dependencies and compute the
execution order, instead of hard-coding the sequence in the renderer. This makes
inserting or reordering passes (deferred shading, HDR, extra post) a data change.

- **Foundation (#226, isolated, tested):** `src/render/RenderPassGraph.h` plus
  `tests/test_render_pass_graph.cpp` - deterministic topological ordering with
  cycle detection, header-only and GL-free.
- **Adoption (#232, done):** `src/render/FrameGraph.h` defines the renderer's real
  pass set as data (`buildDefaultFrameGraph()`:
  `shadow -> scene_begin -> skybox -> forward -> particles -> postprocess`) and a
  `FrameGraphExecutor` that runs each pass's handler in the computed order. The
  main render loop (`src/main.cpp`) now binds a handler per pass and calls
  `execute()` instead of a hard-coded sequence, so reordering/inserting a pass is a
  change to `buildDefaultFrameGraph()`, not to the loop. The graph is built to
  reproduce the exact pre-adoption order, so existing scenes render unchanged;
  `tests/test_frame_graph.cpp` pins that order headlessly.
- **Next:** as later passes land (P2/P3), express them as nodes in
  `buildDefaultFrameGraph()` rather than new inline code in the loop.
- Effort: medium. Risk: low (order is byte-identical to the previous sequence).

### P2 - PBR materials (metallic-roughness)

Move from Blinn-Phong to a metallic-roughness PBR model with image-based lighting.

- Start with header-only, unit-testable BRDF math (GGX distribution, Smith
  geometry, Fresnel-Schlick, sRGB/linear conversion), then a new shader path
  behind a material flag so Phong materials keep rendering unchanged.
- Effort: high. Risk: medium (gated behind a material flag; opt-in per material).

### P3 - HDR pipeline + tone mapping

Render lighting to an HDR target and tone-map (for example ACES) on resolve, with
exposure control. Pairs naturally with P2 and improves bloom quality.

- Isolate as a resolve pass in the P1 graph; keep an LDR fallback.
- Effort: medium. Risk: medium.

### P4 - Shadow and lighting quality

Incremental quality wins: cascaded shadow maps for the directional light, PCF/soft
shadows, and normal-mapping support in the material path.

- Effort: medium per item. Risk: low to medium; each item is independent.

## Explicitly out of scope (for now)

A full deferred/clustered renderer or a Vulkan/RHI backend. These are large
rewrites with little differentiation payoff for this engine and should only be
considered if a concrete need appears.

## Suggested sequencing

P1 foundation (this issue) -> agree on adoption -> P2 BRDF math + opt-in PBR path
-> P3 HDR/tone mapping -> P4 quality passes. Each step ships behind a flag or as an
added pass so existing scenes render unchanged until the step is validated.
