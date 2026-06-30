// Unit test for the ECS query/iteration API (View) - Milestone 8, issue #141.
//
// Verifies the issue's acceptance criteria:
//   1. A system can iterate (Position, Velocity) entities and mutate them.
//   2. Query iteration order is deterministic.
//   3. Queries compose with include/exclude.
// Also checks that a query spans multiple archetypes (any superset matches) and
// that mutations through the view are visible via get().
//
// Pure standard library + the header-only ECS:
//   g++ -std=c++17 -I src tests/test_ecs_query.cpp -o test_ecs_query

#include "core/ecs/ECS.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace IKore::ecs;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

struct Position {
    float x{0}, y{0}, z{0};
};
struct Velocity {
    float dx{0}, dy{0}, dz{0};
};
struct Frozen {
    bool dummy{true};
};
struct Tag {
    int id{0};
};

static void testIterateAndMutate() {
    Registry world;
    Entity a = world.create();
    Entity b = world.create();
    Entity c = world.create();
    world.add<Position>(a, Position{0, 0, 0});
    world.add<Velocity>(a, Velocity{1, 2, 3});
    world.add<Position>(b, Position{10, 10, 10});
    world.add<Velocity>(b, Velocity{1, 1, 1});
    world.add<Position>(c, Position{100, 0, 0}); // Position only -> must NOT match

    int visited = 0;
    world.view<Position, Velocity>().each([&](Entity, Position& p, Velocity& v) {
        p.x += v.dx;
        p.y += v.dy;
        p.z += v.dz;
        ++visited;
    });

    CHECK(visited == 2); // only a and b have both
    CHECK(world.get<Position>(a).x == 1.0f);  // 0 + 1
    CHECK(world.get<Position>(a).z == 3.0f);  // 0 + 3
    CHECK(world.get<Position>(b).x == 11.0f); // 10 + 1
    CHECK(world.get<Position>(c).x == 100.0f); // untouched (not matched)
}

static void testSpansMultipleArchetypes() {
    // Entities with {Position,Velocity} and with {Position,Velocity,Tag} live in
    // different archetypes; a view<Position,Velocity> must visit both.
    Registry world;
    Entity a = world.create();
    Entity b = world.create();
    world.add<Position>(a, Position{});
    world.add<Velocity>(a, Velocity{});
    world.add<Position>(b, Position{});
    world.add<Velocity>(b, Velocity{});
    world.add<Tag>(b, Tag{7}); // moves b to a different archetype

    // Extra parens: the commas are template-argument separators, not macro args.
    CHECK((world.view<Position, Velocity>().count() == 2));
    CHECK((world.view<Position, Velocity, Tag>().count() == 1));
}

static void testExclude() {
    Registry world;
    Entity a = world.create();
    Entity b = world.create();
    world.add<Position>(a, Position{1, 0, 0});
    world.add<Position>(b, Position{2, 0, 0});
    world.add<Frozen>(b, Frozen{}); // b is frozen

    int visited = 0;
    float sumX = 0;
    world.view<Position>().exclude<Frozen>().each([&](Entity, Position& p) {
        ++visited;
        sumX += p.x;
    });
    CHECK(visited == 1);     // only a
    CHECK(sumX == 1.0f);     // a.x

    CHECK(world.view<Position>().count() == 2);                  // both
    CHECK(world.view<Position>().exclude<Frozen>().count() == 1); // only a
}

static void testDeterministicOrder() {
    Registry world;
    std::vector<Entity> made;
    for (int i = 0; i < 32; ++i) {
        Entity e = world.create();
        world.add<Position>(e, Position{static_cast<float>(i), 0, 0});
        if (i % 2 == 0) world.add<Velocity>(e, Velocity{}); // mix archetypes
        made.push_back(e);
    }

    auto collect = [&] {
        std::vector<float> order;
        world.view<Position>().each([&](Entity, Position& p) { order.push_back(p.x); });
        return order;
    };

    const std::vector<float> first = collect();
    const std::vector<float> second = collect();
    CHECK(first.size() == 32);
    CHECK(first == second); // identical order across runs -> deterministic
}

static void testEmptyAndNoMatch() {
    Registry world;
    // No entities at all.
    CHECK(world.view<Position>().count() == 0);

    Entity e = world.create();
    world.add<Velocity>(e, Velocity{});
    // Entities exist, but none match the requested component.
    CHECK(world.view<Position>().count() == 0);
    CHECK(world.view<Velocity>().count() == 1);
}

int main() {
    testIterateAndMutate();
    testSpansMultipleArchetypes();
    testExclude();
    testDeterministicOrder();
    testEmptyAndNoMatch();

    if (g_failures == 0) {
        std::printf("All ECS query tests passed.\n");
        return 0;
    }
    std::printf("%d ECS query test(s) failed.\n", g_failures);
    return 1;
}
