-- Sample gameplay scene driven entirely by Lua (Milestone 10 / #147).
-- Demonstrates the IKore scripting API: entities, components, and events.
-- Run with ScriptSystem::runFile("assets/scripts/sample.lua").
-- Full API reference: docs/SCRIPTING.md.

-- Helper: spawn an entity with a Transform at (x, y, z).
local function spawn_at(x, y, z)
    local e = world.create()
    world.add_transform(e)
    local t = world.get_transform(e)  -- get_transform returns a COPY,
    t.position = Vec3.new(x, y, z)
    world.set_transform(e, t)         -- so write it back with set_transform.
    return e
end

-- Helper: give an entity a linear velocity.
local function set_velocity(e, vx, vy, vz)
    world.add_velocity(e)
    local v = world.get_velocity(e)
    v.linear = Vec3.new(vx, vy, vz)
    world.set_velocity(e, v)
end

-- --- Build a small scene -------------------------------------------------
local player = spawn_at(0, 0, 0)
set_velocity(player, 1, 0, 0)         -- moving along +x

local coins = {}
for i = 1, 3 do
    coins[i] = spawn_at(i * 2, 0, 0)  -- coins lined up ahead of the player
end

local enemy = spawn_at(10, 0, 0)
set_velocity(enemy, -1, 0, 0)         -- moving toward the player

-- --- Event-driven scoring -----------------------------------------------
local score = 0
events.on("coin_collected", function(value)
    score = score + value
    print(string.format("coin collected (+%d), score = %d", value, score))
end)

-- Simulate the player collecting each coin.
for _ = 1, #coins do
    events.emit("coin_collected", 10)
end

-- --- Summary ------------------------------------------------------------
print(string.format("scene ready: %d entities have a transform; final score = %d",
                    world.transform_count(), score))
