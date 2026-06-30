-- Sample IKore script (Milestone 10 / #145) showing the Lua API surface.
-- Run via ScriptSystem::runFile("assets/scripts/sample.lua").

-- Create an entity and give it a transform.
local player = world.create()
world.add_transform(player)

local t = world.get_transform(player)
t.position = Vec3.new(0, 1, 0)
world.set_transform(player, t)

-- Give it a velocity.
world.add_velocity(player, Velocity.new())
local v = world.get_velocity(player)
v.linear = Vec3.new(1, 0, 0)
world.set_velocity(player, v)

-- Subscribe to an event, then emit one.
events.on("player_spawned", function(id)
    print("player_spawned received, id = " .. id)
end)
events.emit("player_spawned", 1)

print("entities with a transform: " .. world.transform_count())
