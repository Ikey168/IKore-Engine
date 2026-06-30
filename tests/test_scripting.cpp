// Test for the Lua scripting layer (sol2 + ecs) - Milestone 10, issue #145.
//
// Runs a Lua script through ScriptSystem and verifies, from the C++ side, that
// it created an entity, wrote/read components, and that an emitted event reached
// a Lua subscriber. Demonstrates the issue's acceptance criteria.
//
// NOTE: requires Lua + sol2 (fetched by CMake). It cannot be built or run in the
// authoring sandbox (no Lua, the proxy blocks fetching it); CI's CodeQL build
// compiles it. Run it locally to exercise the runtime behavior:
//   cmake --build build --target test_scripting && ./build/test_scripting

#include "scripting/ScriptSystem.h"

#include <sol/sol.hpp>

#include <cstdio>
#include <string>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

int main() {
    ScriptSystem script;

    const char* code = R"LUA(
        e = world.create()

        world.add_transform(e)
        local t = world.get_transform(e)
        t.position = Vec3.new(5, 6, 7)
        world.set_transform(e, t)

        world.add_velocity(e, Velocity.new())
        local v = world.get_velocity(e)
        v.linear = Vec3.new(1, 0, 0)
        world.set_velocity(e, v)

        count = world.transform_count()

        received = -1
        events.on("ping", function(n) received = n end)
        events.emit("ping", 42)
    )LUA";

    std::string err;
    const bool ok = script.runString(code, &err);
    CHECK(ok);
    if (!ok) std::printf("script error: %s\n", err.c_str());

    // Verify the script's effects through the C++ ECS registry.
    ecs::Entity e = script.lua()["e"].get<ecs::Entity>();
    CHECK(script.registry().isValid(e));
    CHECK(script.registry().has<ecs::Transform>(e));
    CHECK(script.registry().has<ecs::Velocity>(e));

    const ecs::Transform& t = script.registry().get<ecs::Transform>(e);
    CHECK(t.position.x == 5.0f);
    CHECK(t.position.y == 6.0f);
    CHECK(t.position.z == 7.0f);

    const ecs::Velocity& v = script.registry().get<ecs::Velocity>(e);
    CHECK(v.linear.x == 1.0f);

    // Verify values observed on the Lua side.
    CHECK(script.lua()["count"].get<int>() == 1);
    CHECK(script.lua()["received"].get<double>() == 42.0);

    if (g_failures == 0) {
        std::printf("All scripting tests passed.\n");
        return 0;
    }
    std::printf("%d scripting test(s) failed.\n", g_failures);
    return 1;
}
