-- Hot-reload demo script (Milestone 10 / #146-#147).
-- When the engine watches this file with FileWatcher and re-runs it via
-- ScriptSystem::runFile on every save, editing and saving applies your change
-- live - no restart. See docs/SCRIPTING.md ("Hot reload").

-- Tweak this value and save: the new value prints immediately.
local spawn_count = 5

for _ = 1, spawn_count do
    local e = world.create()
    world.add_transform(e)
end

print("hot_reload.lua ran: spawned " .. spawn_count ..
      " entities (total with a transform now " .. world.transform_count() .. ")")
