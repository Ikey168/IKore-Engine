// Unit test for the data-oriented (archetype/SoA) ECS - Milestone 8, issue #140.
//
// Verifies the issue's acceptance criteria:
//   1. Components for a given archetype are laid out contiguously in memory.
//   2. Adding/removing a component moves the entity between archetypes correctly.
//   3. No leaks or dangling handles when entities are created/destroyed.
//
// Pure standard library + the header-only ECS, so it can be built and run on its
// own:  g++ -std=c++17 -I src tests/test_archetype_ecs.cpp -o test_archetype_ecs

#include "core/ecs/ECS.h"

#include <cstdint>
#include <cstdio>

using namespace IKore::ecs;

static int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);         \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

struct Position {
    float x{0}, y{0}, z{0};
};
struct Velocity {
    float dx{0}, dy{0}, dz{0};
};

// A component that tracks how many live instances exist, to catch leaks and to
// prove relocations move-and-destroy rather than duplicate or drop instances.
struct Tracked {
    static int alive;
    int value{0};
    Tracked() { ++alive; }
    explicit Tracked(int v) : value(v) { ++alive; }
    Tracked(const Tracked& o) : value(o.value) { ++alive; }
    Tracked(Tracked&& o) noexcept : value(o.value) { ++alive; }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) noexcept = default;
    ~Tracked() { --alive; }
};
int Tracked::alive = 0;

static void testBasicAddGet() {
    Registry world;
    Entity e = world.create();
    CHECK(world.isValid(e));
    CHECK(!world.has<Position>(e));

    world.add<Position>(e, Position{1.0f, 2.0f, 3.0f});
    CHECK(world.has<Position>(e));
    CHECK(world.get<Position>(e).x == 1.0f);
    CHECK(world.get<Position>(e).z == 3.0f);

    // add() with an existing component overwrites in place.
    world.add<Position>(e, Position{4.0f, 5.0f, 6.0f});
    CHECK(world.get<Position>(e).y == 5.0f);
}

static void testContiguousStorage() {
    // Two entities with the same signature share an archetype; their component
    // rows must be adjacent in memory (structure-of-arrays).
    Registry world;
    Entity a = world.create();
    Entity b = world.create();
    world.add<Position>(a, Position{10, 0, 0});
    world.add<Position>(b, Position{20, 0, 0});

    auto* pa = &world.get<Position>(a);
    auto* pb = &world.get<Position>(b);
    const std::ptrdiff_t byteGap =
        reinterpret_cast<const char*>(pb) - reinterpret_cast<const char*>(pa);
    CHECK(byteGap == static_cast<std::ptrdiff_t>(sizeof(Position)));
}

static void testArchetypeTransitions() {
    Registry world;
    Entity e = world.create();
    world.add<Position>(e, Position{1, 1, 1});
    world.add<Velocity>(e, Velocity{2, 2, 2});

    // After adding a second component the entity has both, with values intact.
    CHECK(world.has<Position>(e));
    CHECK(world.has<Velocity>(e));
    CHECK(world.get<Position>(e).x == 1.0f);
    CHECK(world.get<Velocity>(e).dy == 2.0f);

    // Removing one moves the entity back to the single-component archetype.
    world.remove<Velocity>(e);
    CHECK(world.has<Position>(e));
    CHECK(!world.has<Velocity>(e));
    CHECK(world.get<Position>(e).x == 1.0f);
}

static void testStableHandlesAcrossRelocation() {
    // Three entities in the {Position} archetype. Promoting the middle one to a
    // new archetype triggers a swap-remove that relocates the last entity into
    // the freed row. All three handles must still resolve to the right data.
    Registry world;
    Entity a = world.create();
    Entity b = world.create();
    Entity c = world.create();
    world.add<Position>(a, Position{1, 0, 0});
    world.add<Position>(b, Position{2, 0, 0});
    world.add<Position>(c, Position{3, 0, 0});

    world.add<Velocity>(b, Velocity{9, 9, 9}); // relocates `c` into b's old row

    CHECK(world.get<Position>(a).x == 1.0f);
    CHECK(world.get<Position>(b).x == 2.0f);
    CHECK(world.get<Position>(c).x == 3.0f);
    CHECK(world.has<Velocity>(b));
    CHECK(!world.has<Velocity>(a));
    CHECK(!world.has<Velocity>(c));
}

static void testGenerationalInvalidation() {
    Registry world;
    Entity e = world.create();
    world.add<Position>(e, Position{7, 7, 7});
    CHECK(world.isValid(e));

    world.destroy(e);
    CHECK(!world.isValid(e)); // stale handle no longer valid

    Entity reused = world.create(); // recycles e's slot with a bumped generation
    CHECK(world.isValid(reused));
    CHECK(reused.index == e.index);
    CHECK(reused.generation != e.generation);
    CHECK(!world.isValid(e)); // old handle still invalid against recycled slot
}

static void testNoLeaks() {
    CHECK(Tracked::alive == 0);
    {
        Registry world;
        std::vector<Entity> entities;
        for (int i = 0; i < 64; ++i) {
            Entity e = world.create();
            world.add<Tracked>(e, Tracked{i});
            if (i % 2 == 0) world.add<Position>(e, Position{}); // force archetype churn
            entities.push_back(e);
        }
        // Add/remove to exercise relocation and overwrite paths.
        for (std::size_t i = 0; i < entities.size(); ++i) {
            world.add<Velocity>(entities[i], Velocity{});
            if (i % 3 == 0) world.remove<Tracked>(entities[i]);
        }
        // Destroy half the entities.
        for (std::size_t i = 0; i < entities.size(); i += 2) {
            world.destroy(entities[i]);
        }
        CHECK(Tracked::alive > 0); // some still live inside the world
    }
    // Registry destroyed -> every remaining Tracked must have been destructed.
    CHECK(Tracked::alive == 0);
}

int main() {
    testBasicAddGet();
    testContiguousStorage();
    testArchetypeTransitions();
    testStableHandlesAcrossRelocation();
    testGenerationalInvalidation();
    testNoLeaks();

    if (g_failures == 0) {
        std::printf("All archetype ECS tests passed.\n");
        return 0;
    }
    std::printf("%d archetype ECS test(s) failed.\n", g_failures);
    return 1;
}
