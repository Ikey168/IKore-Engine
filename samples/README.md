# Samples

Small, self-contained programs that exercise the engine's header-only cores with
no GL context, so they build and run anywhere. Each is also a CMake target.

## Build and run

With CMake (from a build directory):

```
cmake --build . --target sample_vertical_slice
./sample_vertical_slice
```

Or directly with a compiler:

```
g++ -std=c++17 -I src samples/vertical_slice_demo.cpp -o sample_vertical_slice
./sample_vertical_slice
```

## What's here

- `vertical_slice_demo.cpp` (`sample_vertical_slice`): the roadmap vertical slice
  end to end - import a neighborhood from GeoJSON, bake a nav grid, spawn a crowd
  that navigates to a goal, then rewind time and replay it deterministically.
- `benchmark_export_demo.cpp` (`sample_benchmark_export`): capture FPS, frame
  time, and memory over a simulated run and export the samples via the
  deterministic data-export format (CSV to a file, JSON to stdout).
