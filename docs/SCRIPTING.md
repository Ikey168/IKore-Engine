# Scripting (Lua) and Hot Reload

IKore embeds **Lua 5.4** (via [sol2](https://github.com/ThePhd/sol2)) so gameplay
can be authored in scripts instead of C++. The scripting layer
(`src/scripting/ScriptSystem.{h,cpp}`, Milestone 10 / #145) exposes the
data-oriented ECS (`ecs::Registry`) and the engine `EventSystem` to Lua. Live
editing is provided by the hot-reload file watcher (`src/core/FileWatcher.h`,
#146).

> The scripting layer is optional and isolated: including `ScriptSystem.h` does
> not pull in Lua or sol2. Scripts currently operate on a `ScriptSystem`-owned
> `ecs::Registry` (the live engine is not yet driven by the new ECS).

## Running scripts

```cpp
#include "scripting/ScriptSystem.h"

IKore::ScriptSystem scripts;
std::string error;
if (!scripts.runFile("assets/scripts/sample.lua", &error)) {
    // error is also logged; runString(code, &error) runs an inline chunk
}
```

`runString` / `runFile` return `false` and set the optional error string on a
Lua error (the script is run protected, so a script error never throws into C++).

## API reference

### Types

| Lua | Constructor | Fields |
|---|---|---|
| `Vec3` | `Vec3.new()`, `Vec3.new(x, y, z)` | `x`, `y`, `z` (numbers) |
| `Transform` | `Transform.new()` | `position`, `rotation`, `scale` (all `Vec3`) |
| `Velocity` | `Velocity.new()` | `linear`, `angular` (both `Vec3`) |
| `Entity` | (returned by `world.create()`) | `index`, `generation` (read-only) |

### `world` - entities and components

| Function | Returns | Description |
|---|---|---|
| `world.create()` | `Entity` | Create a new entity. |
| `world.destroy(e)` | - | Destroy an entity (its handle becomes invalid). |
| `world.valid(e)` | `bool` | Is the entity handle still valid? |
| `world.add_transform(e [, t])` | - | Add a `Transform` (default if `t` omitted). |
| `world.has_transform(e)` | `bool` | Does the entity have a `Transform`? |
| `world.get_transform(e)` | `Transform` | **A copy** of the entity's transform. |
| `world.set_transform(e, t)` | - | Write a `Transform` back to the entity. |
| `world.add_velocity(e [, v])` | - | Add a `Velocity` (default if `v` omitted). |
| `world.has_velocity(e)` | `bool` | Does the entity have a `Velocity`? |
| `world.get_velocity(e)` | `Velocity` | **A copy** of the entity's velocity. |
| `world.set_velocity(e, v)` | - | Write a `Velocity` back to the entity. |
| `world.transform_count()` | `int` | Number of entities that have a `Transform`. |

> **Important:** `get_transform` / `get_velocity` return a **copy**. To change a
> component, get it, mutate the copy, then write it back with the matching
> `set_*`:
> ```lua
> local t = world.get_transform(e)
> t.position = Vec3.new(1, 2, 3)
> world.set_transform(e, t)
> ```

### `events` - the EventSystem bridge

| Function | Description |
|---|---|
| `events.emit(name)` | Publish event `name` with no payload. |
| `events.emit(name, number)` | Publish event `name` with a numeric payload. |
| `events.on(name, fn)` | Subscribe; `fn(number)` is called on each emit (payload, or 0). |

```lua
events.on("coin_collected", function(value)
    print("score += " .. value)
end)
events.emit("coin_collected", 10)
```

Subscriptions are owned by the `ScriptSystem` and released when it is destroyed.

## Hot reload

`FileWatcher` (`src/core/FileWatcher.h`) re-runs scripts (and reloads shaders or
textures) when their files change on disk. Register a file with a reload
callback and call `poll()` once per frame:

```cpp
#include "core/FileWatcher.h"
#include "scripting/ScriptSystem.h"

IKore::ScriptSystem scripts;
IKore::FileWatcher  watcher;

watcher.watch("assets/scripts/hot_reload.lua",
              [&](const std::string& path) { scripts.runFile(path); });

// each frame:
watcher.poll();   // edits to the .lua re-run it live, no restart
```

- The callback fires when the file's modification time advances (or a missing
  file appears).
- A reload that fails (a script error, a bad shader) is **caught and reported**
  through the watcher's error handler; `poll()` keeps going and the engine does
  not crash.
- The watcher is portable, dependency-free polling (no inotify / OS-specific
  APIs), so the same code runs on every platform. Set a custom reporter with
  `watcher.setErrorHandler(...)`.

## Sample scripts

- [`assets/scripts/sample.lua`](../assets/scripts/sample.lua) - a small scene:
  spawns a player, coins, and an enemy; sets transforms/velocities; and wires an
  event-driven score. Demonstrates entity, component, and event usage end to end.
- [`assets/scripts/hot_reload.lua`](../assets/scripts/hot_reload.lua) - a minimal
  script to edit while the engine runs, to see hot reload in action.

## Building and testing

The `test_scripting` CMake target builds the bindings and runs the scripted
scenario against the C++ registry:

```bash
cmake -S . -B build && cmake --build build --target test_scripting
./build/test_scripting
```

Lua and sol2 are fetched automatically by CMake (see the top-level
`CMakeLists.txt`).
