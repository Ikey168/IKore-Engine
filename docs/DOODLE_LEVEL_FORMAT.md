# Doodlebound level and scene formats

Milestone 15 defines two small JSON formats and the conversion between them. Both
are pure data, have no renderer dependency, and round-trip through save/load.

- `doodle-level` - the intermediate, hand-authorable level spec: wall polylines
  plus placed symbols. A drawing front end (or a person, by hand) emits this.
- `doodle-scene` - the emitted 3D scene the engine loads: oriented wall boxes plus
  entity spawns. It is produced by converting a level spec.

Pipeline:

```
doodle-level JSON  --fromLevelJson-->  LevelSpec
LevelSpec          --convert-------->  SceneDescription   (extruded geometry + spawns)
SceneDescription   --toSceneJson---->  doodle-scene JSON
SceneDescription   --emitToRegistry->  ECS entities       (a playable scene)
```

All types live in `IKore::game` (`game/DoodleScene.h`, `game/LevelFormat.h`).

## doodle-level

Coordinates are on the XZ ground plane (the drawing plane); walls rise along +Y.

| Field           | Type     | Meaning                                              |
| --------------- | -------- | ---------------------------------------------------- |
| `format`        | string   | `"doodle-level"`.                                    |
| `version`       | integer  | Schema version (currently `1`).                      |
| `wallHeight`    | number   | Extrusion height for every wall (world units).       |
| `wallThickness` | number   | Wall box thickness (world units).                    |
| `walls`         | array    | Wall polylines (see below).                          |
| `symbols`       | array    | Placed symbols (see below).                          |

A wall is an object with a `polyline`: an array of `[x, z]` points. Each
consecutive pair of points becomes one extruded wall segment. A closed room
repeats its first point at the end. Zero-length segments are ignored.

A symbol is an object:

| Field  | Type   | Meaning                                          |
| ------ | ------ | ------------------------------------------------ |
| `type` | string | `"player"`, `"enemy"`, `"treasure"`, `"door"`, ... |
| `x`    | number | X position on the ground plane.                  |
| `z`    | number | Z position on the ground plane.                  |
| `yaw`  | number | Optional facing in radians (default `0`).        |

Example (see `assets/levels/sample_dungeon.level.json`):

```json
{
  "format": "doodle-level",
  "version": 1,
  "wallHeight": 3.0,
  "wallThickness": 0.2,
  "walls": [
    {"polyline": [[0, 0], [20, 0], [20, 15], [0, 15], [0, 0]]},
    {"polyline": [[8, 0], [8, 9]]}
  ],
  "symbols": [
    {"type": "player", "x": 2, "z": 2, "yaw": 0},
    {"type": "enemy", "x": 16, "z": 4},
    {"type": "treasure", "x": 18, "z": 13}
  ]
}
```

## doodle-scene

The converter extrudes each wall segment into an oriented box and turns each
symbol into an entity spawn.

| Field    | Type    | Meaning                          |
| -------- | ------- | -------------------------------- |
| `format` | string  | `"doodle-scene"`.                |
| `version`| integer | Schema version (currently `1`).  |
| `walls`  | array   | Oriented wall boxes (see below). |
| `spawns` | array   | Entity spawns (see below).       |

A wall box has `center` `[x, y, z]`, `size` `[sx, sy, sz]`, and a `yaw` (radians,
about the Y axis). A spawn has `type`, `position` `[x, y, z]`, and `yaw`.

The triangle mesh produced by `convert` is not stored in the scene JSON; it is
derived deterministically from the wall boxes, so the boxes are the canonical
geometry. `emitToRegistry` creates one entity per wall box (with a `Transform`)
and one per spawn (a `Transform` plus a `SpawnTag` carrying its type).

## Round-tripping

`toLevelJson` / `fromLevelJson` and `toSceneJson` / `fromSceneJson` are inverses
for the fields above, so a level or scene can be saved and reloaded without loss.
