#pragma once

#include "core/EventSystem.h"
#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file ScriptSystem.h
 * @brief Lua scripting layer (sol2) bound to the data-oriented ECS + EventSystem.
 *
 * Milestone 10 (issue #145). Embeds a Lua VM and exposes a small, clean API so
 * scripts can create entities, read/write components, and emit/subscribe to
 * events. The implementation detail (sol2/Lua) is hidden behind this header
 * (sol::state is held via a forward-declared pimpl), so including this file does
 * not pull in Lua or sol2 - the scripting layer stays optional and isolated.
 *
 * Scripts operate on a ScriptSystem-owned ecs::Registry. (The live engine is not
 * yet driven by the new ECS - that is the deferred #142 follow-on - so for now
 * scripting drives its own world.)
 *
 * Lua API exposed (see ScriptSystem.cpp for the bindings):
 *   world.create() -> Entity
 *   world.destroy(e) / world.valid(e)
 *   world.add_transform(e[, Transform]) / has_transform(e) / get_transform(e) / set_transform(e, t)
 *   world.add_velocity(e[, Velocity]) / has_velocity(e) / get_velocity(e) / set_velocity(e, v)
 *   world.transform_count()
 *   Vec3.new(x,y,z) / Transform.new() / Velocity.new()
 *   events.emit(name[, number]) / events.on(name, function(number))
 */
namespace sol { class state; }

namespace IKore {

class ScriptSystem {
public:
    ScriptSystem();
    ~ScriptSystem();

    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem& operator=(const ScriptSystem&) = delete;

    /// Run a chunk of Lua source. Returns false and sets @p errorOut on error.
    bool runString(const std::string& code, std::string* errorOut = nullptr);

    /// Run a Lua file. Returns false and sets @p errorOut on error.
    bool runFile(const std::string& path, std::string* errorOut = nullptr);

    /// The world that scripts manipulate.
    ecs::Registry& registry() { return m_registry; }

    /// Access the underlying Lua state (for host-side inspection/tests).
    sol::state& lua() { return *m_lua; }

private:
    void registerBindings();

    ecs::Registry m_registry;
    std::unique_ptr<sol::state> m_lua;
    std::vector<EventSubscription> m_subscriptions; // event hooks to release on destroy
};

} // namespace IKore
