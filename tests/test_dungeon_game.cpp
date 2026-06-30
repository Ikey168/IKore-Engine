// Test for the playable top-down dungeon loop - Milestone 15, #164.
//
// Verifies the issue's acceptance criterion that a JSON-authored map is playable
// end-to-end: move (with wall collision), collect coins, avoid an enemy, and win
// by reaching the exit. Built headless on the converted scene (#162/#163).
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_dungeon_game.cpp -o test_dungeon_game

#include "game/DoodleScene.h"
#include "game/DungeonGame.h"

#include <cmath>
#include <cstdio>

using namespace IKore;
using namespace IKore::game;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static const float kDt = 1.0f / 60.0f;

// Drive the player toward a target, sliding around walls, until close or timeout.
static void driveToward(DungeonGame& g, const ecs::Vec3& target, int maxSteps) {
    for (int i = 0; i < maxSteps && g.status == GameStatus::Playing; ++i) {
        const float dx = target.x - g.playerPosition.x;
        const float dz = target.z - g.playerPosition.z;
        if (std::sqrt(dx * dx + dz * dz) < 0.05f) break;
        g.update(GameInput{dx, dz}, kDt);
    }
}

static SceneDescription buildScene(const LevelSpec& spec) { return convert(spec); }

// An open room with two coins on the path, an exit, and a distant enemy.
static LevelSpec openLevel() {
    LevelSpec spec;
    Wall room;
    room.polyline = {{0, 0, 0}, {12, 0, 0}, {12, 0, 8}, {0, 0, 8}, {0, 0, 0}};
    spec.walls.push_back(room);
    spec.symbols = {
        {"player", {1, 0, 4}, 0.0f},
        {"treasure", {4, 0, 4}, 0.0f},
        {"treasure", {7, 0, 4}, 0.0f},
        {"door", {10, 0, 4}, 0.0f},
        {"enemy", {1, 0, 7}, 0.0f},
    };
    return spec;
}

static void testPlayableEndToEnd() {
    DungeonGame g = loadGame(buildScene(openLevel()));
    g.enemySpeed = 1.0f; // clearly slower than the player so the run is winnable

    CHECK(g.totalCoins == 2);
    CHECK(g.hasExit);
    CHECK(g.enemies.size() == 1);
    const float startX = g.playerPosition.x;

    // Collect both coins, then reach the exit.
    driveToward(g, ecs::Vec3{4, 0, 4}, 600);
    driveToward(g, ecs::Vec3{7, 0, 4}, 600);
    driveToward(g, ecs::Vec3{10, 0, 4}, 600);

    CHECK(g.playerPosition.x > startX + 1.0f); // the player actually moved
    CHECK(g.coinsCollected == 2);              // collected
    CHECK(g.coinsRemaining() == 0);
    CHECK(g.won());                            // reached the exit with all coins
}

static void testWallCollisionBlocksMovement() {
    LevelSpec spec;
    Wall wall;
    wall.polyline = {{6, 0, 0}, {6, 0, 8}}; // vertical wall at x=6
    spec.walls.push_back(wall);
    spec.symbols = {{"player", {3, 0, 4}, 0.0f}};

    DungeonGame g = loadGame(buildScene(spec));
    CHECK(!g.hitsWall(g.playerPosition, g.playerRadius)); // valid start

    for (int i = 0; i < 400; ++i) g.update(GameInput{1.0f, 0.0f}, kDt); // push right into the wall

    CHECK(g.playerPosition.x > 3.0f);  // moved toward the wall
    CHECK(g.playerPosition.x < 5.7f);  // but did not pass through it (wall at x~6)
    CHECK(!g.hitsWall(g.playerPosition, g.playerRadius)); // never ended up inside a wall
}

static void testEnemyContactLoses() {
    LevelSpec spec;
    spec.symbols = {{"player", {4, 0, 4}, 0.0f}, {"enemy", {5, 0, 4}, 0.0f}}; // adjacent
    DungeonGame g = loadGame(buildScene(spec));

    // Stand still; the enemy closes in and catches the player.
    for (int i = 0; i < 120 && g.status == GameStatus::Playing; ++i) {
        g.update(GameInput{0.0f, 0.0f}, kDt);
    }
    CHECK(g.lost());
    CHECK(!g.won());
}

static void testExitRequiresAllCoins() {
    LevelSpec spec;
    Wall room;
    room.polyline = {{0, 0, 0}, {12, 0, 0}, {12, 0, 8}, {0, 0, 8}, {0, 0, 0}};
    spec.walls.push_back(room);
    spec.symbols = {
        {"player", {1, 0, 4}, 0.0f},
        {"treasure", {4, 0, 1}, 0.0f}, // off the direct path to the exit
        {"door", {10, 0, 4}, 0.0f},
    };
    DungeonGame g = loadGame(buildScene(spec));

    // Reach the exit without grabbing the coin: not a win yet.
    driveToward(g, ecs::Vec3{10, 0, 4}, 600);
    CHECK(!g.won());
    CHECK(g.coinsRemaining() == 1);

    // Grab the coin, then return to the exit to win.
    driveToward(g, ecs::Vec3{4, 0, 1}, 600);
    CHECK(g.coinsRemaining() == 0);
    driveToward(g, ecs::Vec3{10, 0, 4}, 600);
    CHECK(g.won());
}

int main() {
    testPlayableEndToEnd();
    testWallCollisionBlocksMovement();
    testEnemyContactLoses();
    testExitRequiresAllCoins();

    if (g_failures == 0) {
        std::printf("All dungeon game tests passed.\n");
        return 0;
    }
    std::printf("%d dungeon game test(s) failed.\n", g_failures);
    return 1;
}
