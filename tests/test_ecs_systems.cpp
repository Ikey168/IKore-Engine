// Unit test for the migrated hot-path components + systems - Milestone 8, #142.
//
// Verifies the issue's acceptance criteria:
//   - The migrated components (Transform/Velocity/Physics/AI) run against the new
//     archetype storage via the systems.
//   - No behavioral regressions: movement, physics integration, and AI steering
//     produce the expected results, deterministically.
//   - The compatibility bridge (Legacy) carries less-frequent/legacy components
//     through archetype moves without leaking or dangling.
//
// Pure standard library + the header-only ECS:
//   g++ -std=c++17 -I src tests/test_ecs_systems.cpp -o test_ecs_systems

#include "core/ecs/components/Components.h"
#include "core/ecs/systems/Systems.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace IKore::ecs;

static int g_failures = 0;

#define CHECK(cond)                                               \
    do {                                                          \
        if (!(cond)) {                                            \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond); \
            ++g_failures;                                         \
        }                                                         \
    } while (0)

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static void testMovement() {
    Registry world;
    Entity e = world.create();
    world.add<Transform>(e, Transform{});
    world.add<Velocity>(e, Velocity{Vec3{2.0f, 0.0f, 0.0f}, Vec3{}});

    movementSystem(world, 0.5f);
    CHECK(approx(world.get<Transform>(e).position.x, 1.0f)); // 0 + 2*0.5
    movementSystem(world, 0.5f);
    CHECK(approx(world.get<Transform>(e).position.x, 2.0f)); // + 2*0.5
}

static void testPhysicsIntegration() {
    Registry world;
    Entity body = world.create();
    world.add<Velocity>(body, Velocity{});
    world.add<RigidBody>(body, RigidBody{Vec3{0.0f, -10.0f, 0.0f}, 1.0f, 0.0f, false});

    physicsSystem(world, 0.1f);
    CHECK(approx(world.get<Velocity>(body).linear.y, -1.0f)); // 0 + (-10)*0.1

    // Kinematic bodies are not integrated.
    Entity kin = world.create();
    world.add<Velocity>(kin, Velocity{});
    world.add<RigidBody>(kin, RigidBody{Vec3{0.0f, -10.0f, 0.0f}, 1.0f, 0.0f, true});
    physicsSystem(world, 0.1f);
    CHECK(approx(world.get<Velocity>(kin).linear.y, 0.0f));

    // Damping reduces velocity.
    Entity damped = world.create();
    world.add<Velocity>(damped, Velocity{Vec3{10.0f, 0.0f, 0.0f}, Vec3{}});
    world.add<RigidBody>(damped, RigidBody{Vec3{}, 1.0f, 1.0f, false});
    physicsSystem(world, 0.1f); // factor = 1 - 1*0.1 = 0.9
    CHECK(approx(world.get<Velocity>(damped).linear.x, 9.0f));
}

static void testAISteeringConverges() {
    Registry world;
    Entity agent = world.create();
    world.add<Transform>(agent, Transform{});
    world.add<Velocity>(agent, Velocity{});
    // arriveRadius (0.6) > speed*dt (5 * 0.1 = 0.5) so it settles without oscillating.
    world.add<AIAgent>(agent, AIAgent{Vec3{10.0f, 0.0f, 0.0f}, 5.0f, 0.6f, true});

    for (int i = 0; i < 60; ++i) stepSimulation(world, 0.1f);

    const Transform& t = world.get<Transform>(agent);
    const Vec3 toTarget = Vec3{10.0f, 0.0f, 0.0f} - t.position;
    CHECK(toTarget.length() <= 0.6f);                          // arrived
    CHECK(approx(world.get<Velocity>(agent).linear.length(), 0.0f)); // and stopped
    CHECK(t.position.x <= 10.0f + 1e-3f);                      // no wild overshoot
}

static void testDeterministic() {
    auto run = [] {
        Registry world;
        std::vector<Entity> es;
        for (int i = 0; i < 16; ++i) {
            Entity e = world.create();
            world.add<Transform>(e, Transform{});
            world.add<Velocity>(e, Velocity{Vec3{static_cast<float>(i), 1.0f, 0.0f}, Vec3{}});
            es.push_back(e);
        }
        for (int s = 0; s < 10; ++s) movementSystem(world, 0.1f);
        std::vector<float> out;
        for (Entity e : es) out.push_back(world.get<Transform>(e).position.x);
        return out;
    };
    CHECK(run() == run());
}

static void testLegacyBridgeSurvivesArchetypeMoves() {
    Registry world;
    auto legacyObject = std::make_shared<int>(42); // stand-in for a legacy component
    CHECK(legacyObject.use_count() == 1);

    Entity e = world.create();
    world.add<Transform>(e, Transform{});
    world.add<Legacy>(e, Legacy{legacyObject});
    CHECK(legacyObject.use_count() == 2); // held by the Legacy component too

    // Adding another component relocates the entity to a new archetype.
    world.add<Velocity>(e, Velocity{});
    CHECK(world.get<Legacy>(e).handle.get() == legacyObject.get());
    CHECK(*std::static_pointer_cast<int>(world.get<Legacy>(e).handle) == 42);
    CHECK(legacyObject.use_count() == 2); // moved, not copied, through the relocation

    world.destroy(e);
    CHECK(legacyObject.use_count() == 1); // released on destroy - no leak/dangling
}

int main() {
    testMovement();
    testPhysicsIntegration();
    testAISteeringConverges();
    testDeterministic();
    testLegacyBridgeSurvivesArchetypeMoves();

    if (g_failures == 0) {
        std::printf("All ECS systems/migration tests passed.\n");
        return 0;
    }
    std::printf("%d ECS systems/migration test(s) failed.\n", g_failures);
    return 1;
}
