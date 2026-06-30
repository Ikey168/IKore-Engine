#pragma once

#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"
#include "world/SvgFloorPlanImporter.h" // world::Box / FloorPlan / emitToRegistry

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file DoodleScene.h
 * @brief Renderer-agnostic drawing -> 3D scene converter (issue #162).
 *
 * Milestone 15 (Doodle Dungeon) core. Converts a LevelSpec - the output of a
 * hand-drawn floor plan: wall polylines plus placed symbols - into a 3D scene
 * description: extruded wall geometry and a list of entity spawns.
 *
 * It deliberately has NO renderer dependency so it is unit-testable headless: it
 * emits a plain Mesh (positions, normals, indices - exactly the vertex/index data
 * a MeshComponent builder consumes) and reuses the glm-free world::Box /
 * FloorPlan / emitToRegistry primitives, rather than depending on the glm- and
 * OpenGL-coupled MeshComponent / SerializationData directly (which would break the
 * headless requirement, the same reason the ECS uses a local Vec3 instead of glm).
 *
 * Header-only and dependency-free (std + the header-only ECS / world primitives).
 */
namespace IKore {
namespace game {

/// A wall as a polyline of XZ points (y is ignored; walls rise along +Y).
struct Wall {
    std::vector<ecs::Vec3> polyline;
};

/// A placed symbol (e.g. "player", "enemy", "door", "treasure") at a position.
struct Symbol {
    std::string type;
    ecs::Vec3 position{};
    float yaw{0.0f};
};

/// The drawing: wall polylines + symbols, with extrusion parameters.
struct LevelSpec {
    std::vector<Wall> walls;
    std::vector<Symbol> symbols;
    float wallHeight{3.0f};
    float wallThickness{0.2f};
};

/// A renderer-agnostic mesh vertex (position + normal).
struct MeshVertex {
    ecs::Vec3 position{};
    ecs::Vec3 normal{};
};

/// Plain triangle mesh data, consumable by any renderer's mesh builder.
struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

/// An entity to spawn from a symbol.
struct EntitySpawn {
    std::string type;
    ecs::Vec3 position{};
    float yaw{0.0f};
};

/// ECS tag carrying a spawned entity's symbol type (so spawns are queryable).
struct SpawnTag {
    std::string type;
};

/// The converted scene: extruded wall geometry (mesh + boxes) and entity spawns.
struct SceneDescription {
    Mesh wallMesh;                      ///< all walls extruded into one mesh.
    std::vector<world::Box> wallBoxes;  ///< the same walls as oriented boxes.
    std::vector<EntitySpawn> spawns;    ///< symbols turned into entity spawns.
};

namespace detail {

/// Append an oriented box (24 verts / 36 indices, outward normals) to @p mesh.
inline void appendBox(Mesh& mesh, const ecs::Vec3& center, float sx, float sy, float sz, float yaw) {
    const float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    const float cs = std::cos(yaw), sn = std::sin(yaw);
    auto place = [&](float lx, float ly, float lz) {
        return ecs::Vec3{center.x + (lx * cs - lz * sn), center.y + ly, center.z + (lx * sn + lz * cs)};
    };
    auto rot = [&](float nx, float nz) { return ecs::Vec3{nx * cs - nz * sn, 0.0f, nx * sn + nz * cs}; };

    struct Face {
        ecs::Vec3 n;
        ecs::Vec3 v[4];
    };
    const Face faces[6] = {
        {rot(1, 0), {place(hx, -hy, -hz), place(hx, hy, -hz), place(hx, hy, hz), place(hx, -hy, hz)}},
        {rot(-1, 0), {place(-hx, -hy, hz), place(-hx, hy, hz), place(-hx, hy, -hz), place(-hx, -hy, -hz)}},
        {rot(0, 1), {place(-hx, -hy, hz), place(-hx, hy, hz), place(hx, hy, hz), place(hx, -hy, hz)}},
        {rot(0, -1), {place(hx, -hy, -hz), place(hx, hy, -hz), place(-hx, hy, -hz), place(-hx, -hy, -hz)}},
        {{0.0f, 1.0f, 0.0f}, {place(-hx, hy, -hz), place(-hx, hy, hz), place(hx, hy, hz), place(hx, hy, -hz)}},
        {{0.0f, -1.0f, 0.0f}, {place(-hx, -hy, hz), place(-hx, -hy, -hz), place(hx, -hy, -hz), place(hx, -hy, hz)}},
    };
    for (const Face& f : faces) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (int i = 0; i < 4; ++i) mesh.vertices.push_back(MeshVertex{f.v[i], f.n});
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }
}

} // namespace detail

/**
 * @brief Convert a LevelSpec into a 3D scene description.
 *
 * Each wall polyline segment is extruded from the floor (y = 0) to wallHeight as
 * an oriented box, added both to the merged wall mesh and as a world::Box. Each
 * symbol becomes an EntitySpawn at its position.
 */
inline SceneDescription convert(const LevelSpec& spec) {
    SceneDescription out;
    const float height = spec.wallHeight > 0.0f ? spec.wallHeight : 1.0f;
    const float thickness = spec.wallThickness > 0.0f ? spec.wallThickness : 0.1f;

    for (const Wall& wall : spec.walls) {
        for (std::size_t i = 0; i + 1 < wall.polyline.size(); ++i) {
            const ecs::Vec3 a = wall.polyline[i];
            const ecs::Vec3 b = wall.polyline[i + 1];
            const float dx = b.x - a.x, dz = b.z - a.z;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len < 1e-6f) continue; // skip degenerate segment
            const float yaw = std::atan2(dz, dx);
            const ecs::Vec3 center{(a.x + b.x) * 0.5f, height * 0.5f, (a.z + b.z) * 0.5f};

            world::Box box;
            box.center = center;
            box.size = ecs::Vec3{len, height, thickness};
            box.yaw = yaw;
            out.wallBoxes.push_back(box);
            detail::appendBox(out.wallMesh, center, len, height, thickness, yaw);
        }
    }

    for (const Symbol& s : spec.symbols) {
        out.spawns.push_back(EntitySpawn{s.type, s.position, s.yaw});
    }
    return out;
}

/**
 * @brief Place a converted scene into an ecs::Registry: one entity per wall box
 *        (via the importer emit path) and one per spawn (Transform + SpawnTag).
 * @return the number of entities created.
 */
inline std::size_t emitToRegistry(const SceneDescription& scene, ecs::Registry& registry) {
    world::FloorPlan plan;
    plan.walls = scene.wallBoxes;
    plan.hasFloor = false;
    std::size_t created = world::emitToRegistry(plan, registry);

    for (const EntitySpawn& s : scene.spawns) {
        const ecs::Entity e = registry.create();
        registry.add<ecs::Transform>(
            e, ecs::Transform{s.position, ecs::Vec3{0.0f, s.yaw, 0.0f}, ecs::Vec3{1.0f, 1.0f, 1.0f}});
        registry.add<SpawnTag>(e, SpawnTag{s.type});
        ++created;
    }
    return created;
}

} // namespace game
} // namespace IKore
