# ECS Storage Benchmark: Legacy vs. Archetype (Milestone 8 / #143)

This benchmark quantifies the payoff of the data-oriented ECS (issues
#140-#142) by comparing it against the engine's legacy component model on the
canonical hot-path workload.

## What is compared

| | Legacy model | Archetype ECS |
|---|---|---|
| Storage | Heap-allocated entities, each owning `std::unordered_map<std::type_index, std::shared_ptr<void>>` | Components packed in contiguous, per-type SoA columns grouped by signature |
| Component access | Hash lookup + `shared_ptr` indirection into scattered heap objects | Direct walk of contiguous arrays via `view<...>().each()` |
| Source | mirrors `src/core/Entity.h` exactly | `IKore::ecs::Registry` (`src/core/ecs/`) |

The legacy path is reproduced inside the benchmark (rather than `#include`-ing
`Entity.h`, which pulls in `glm`) so the benchmark is dependency-free and
runnable in isolation. The data structures and access pattern are identical to
the real `Entity`, so the cost characteristics match.

## Workload

Integrate velocity into position for every entity (`pos += vel * dt`), repeated
for N frames, at two entity counts. A checksum of the resulting positions is
printed for both models and must match (it does), proving the work is real and
equivalent and isn't elided by the optimizer.

## Methodology

- **Iteration time**: `std::chrono::steady_clock` around the frame loop only
  (construction excluded). Reported as total ms and ns per component update.
- **Memory**: resident set size (RSS) delta from `/proc/self/statm`, measured
  around each model's construction. `malloc_trim(0)` is called before each
  baseline so freed pages are returned to the OS and the second model's reading
  is not skewed by the allocator reusing the first model's pages (glibc/Linux).
- The build is `-O2 -DNDEBUG`. Absolute timings are machine-dependent; the
  **speedup ratio is the headline** and is stable across runs.

## Results

Representative run (`-O2`, 50 frames; release build, single core):

| Entities | Model | Iter (ms) | ns/update | Memory (MiB) |
|---:|---|---:|---:|---:|
| 100,000 | legacy | 273.3 | 54.67 | 41.3 |
| 100,000 | **archetype** | **5.5** | **1.10** | **6.9** |
| 1,000,000 | legacy | 3563.2 | 71.26 | 412.0 |
| 1,000,000 | **archetype** | **59.7** | **1.19** | **61.8** |

**Headline:** the archetype storage iterates roughly **40-60x faster** and uses
about **6-7x less memory** at high entity counts. The per-update cost is roughly
flat for the archetype model (~1.1 ns/update) but grows for the legacy model as
working sets exceed cache (45-70+ ns/update), because every entity costs a hash
lookup plus pointer chases into scattered heap allocations.

(Exact numbers vary with machine and load - e.g. legacy iteration was measured
between ~45 and ~71 ns/update across runs - but the archetype model is
consistently ~1.1 ns/update and ~40-60x faster.)

## Reproduce

```bash
g++ -std=c++17 -O2 -DNDEBUG -I src tests/test_ecs_benchmark.cpp -o test_ecs_benchmark
./test_ecs_benchmark           # 50 frames per size
./test_ecs_benchmark 200       # optional: pass a frame count
```

Or via CMake (the `test_ecs_benchmark` target builds it; build in Release for
representative timings):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target test_ecs_benchmark
./build/test_ecs_benchmark
```

The program exits non-zero if the archetype model is not faster than the legacy
model, so it also serves as a coarse performance-regression guard.

## Takeaway

The measured speedup validates the motivation for Milestone 8: the data-oriented
(archetype/SoA) ECS removes per-entity hashing and pointer-chasing from the hot
path, which is what makes the large-scale agent simulation in later milestones
(thousands of agents at frame rate, #152-#155) viable.
