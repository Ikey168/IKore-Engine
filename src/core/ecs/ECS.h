#pragma once

/**
 * @file ECS.h
 * @brief Umbrella header for the data-oriented (archetype/SoA) ECS.
 *
 * Milestone 8 — Foundations: Data-Oriented ECS (issue #140).
 *
 * This module provides archetype-based, structure-of-arrays component storage
 * with stable, generational entity handles:
 *
 *   - IKore::ecs::Entity   — a stable handle (index + generation).
 *   - IKore::ecs::Registry — the world: create/destroy entities, add/remove/get
 *                            components, with data stored contiguously per
 *                            component type and grouped by component signature.
 *
 * Example:
 * @code
 * using namespace IKore::ecs;
 * struct Position { float x, y, z; };
 * struct Velocity { float dx, dy, dz; };
 *
 * Registry world;
 * Entity e = world.create();
 * world.add<Position>(e, {0, 0, 0});
 * world.add<Velocity>(e, {1, 0, 0});   // entity moves to the {Position,Velocity} archetype
 * world.get<Position>(e).x += world.get<Velocity>(e).dx;
 * world.remove<Velocity>(e);           // moves back to the {Position} archetype
 * world.destroy(e);                    // handle `e` is now invalid
 * @endcode
 *
 * Roadmap: a cache-friendly query/iteration API is issue #141; migrating the
 * engine's existing components onto this storage is issue #142; benchmarking is
 * issue #143.
 */
#include "core/ecs/Archetype.h"
#include "core/ecs/Component.h"
#include "core/ecs/Entity.h"
#include "core/ecs/Registry.h"
