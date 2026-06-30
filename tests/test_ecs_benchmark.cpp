// ECS storage benchmark - Milestone 8, issue #143.
//
// Compares the engine's legacy component model against the new archetype/SoA ECS
// (issues #140-#142) on the canonical hot-path workload: integrate velocity into
// position for every entity, repeated for many frames.
//
//   Legacy model  - mirrors src/core/Entity.h exactly: heap-allocated entities,
//                   each owning an std::unordered_map<std::type_index,
//                   std::shared_ptr<void>>; components are separate heap objects
//                   reached via a hash lookup + shared_ptr indirection.
//   Archetype ECS - IKore::ecs::Registry: components packed in contiguous SoA
//                   columns, iterated with view<...>().each().
//
// Reports iteration throughput and resident memory at large entity counts. The
// legacy path is reproduced here (rather than #include'd) so the benchmark stays
// dependency-free and runnable in isolation; the data structures and access
// pattern are identical to the real Entity, so the cost characteristics match.
//
// Build (optimized) and run:
//   g++ -std=c++17 -O2 -I src tests/test_ecs_benchmark.cpp -o test_ecs_benchmark
//   ./test_ecs_benchmark

#include "core/ecs/Registry.h"
#include "core/ecs/View.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif
#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
#endif

namespace {

constexpr float kDt = 1.0f / 60.0f;

// Return freed pages to the OS so the next RSS baseline is clean (otherwise the
// allocator reuses just-freed pages and the second model's memory under-reports).
void trimHeap() {
#if defined(__linux__) && defined(__GLIBC__)
    malloc_trim(0);
#endif
}

// ---- Shared component payloads (identical size for a fair comparison) --------
struct Position {
    float x{0.0f}, y{0.0f}, z{0.0f};
};
struct Velocity {
    float x{1.0f}, y{1.0f}, z{1.0f};
};

// ---- Legacy model: a faithful replica of src/core/Entity.h's storage --------
struct LegacyComponent {
    virtual ~LegacyComponent() = default;
};
struct LegacyPosition : LegacyComponent {
    float x{0.0f}, y{0.0f}, z{0.0f};
};
struct LegacyVelocity : LegacyComponent {
    float x{1.0f}, y{1.0f}, z{1.0f};
};

class LegacyEntity : public std::enable_shared_from_this<LegacyEntity> {
public:
    virtual ~LegacyEntity() = default;

    template <class T, class... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        m_components[std::type_index(typeid(T))] = component;
        return component;
    }

    template <class T>
    std::shared_ptr<T> getComponent() {
        auto it = m_components.find(std::type_index(typeid(T)));
        if (it != m_components.end()) return std::static_pointer_cast<T>(it->second);
        return nullptr;
    }

    std::uint64_t id{0};

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_components;
};

// ---- Helpers ----------------------------------------------------------------
using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Current resident set size in MiB (Linux only; 0 elsewhere).
double residentMiB() {
#if defined(__linux__)
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0.0;
    long totalPages = 0, residentPages = 0;
    if (std::fscanf(f, "%ld %ld", &totalPages, &residentPages) != 2) {
        std::fclose(f);
        return 0.0;
    }
    std::fclose(f);
    const long pageSize = sysconf(_SC_PAGESIZE);
    return static_cast<double>(residentPages) * static_cast<double>(pageSize) / (1024.0 * 1024.0);
#else
    return 0.0;
#endif
}

struct Result {
    double iterMs{0.0};
    double memMiB{0.0};
    double checksum{0.0};
};

Result benchmarkLegacy(std::size_t count, int frames) {
    trimHeap();
    const double before = residentMiB();
    std::vector<std::shared_ptr<LegacyEntity>> entities;
    entities.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto e = std::make_shared<LegacyEntity>();
        e->id = i + 1;
        e->addComponent<LegacyPosition>();
        e->addComponent<LegacyVelocity>();
        entities.push_back(std::move(e));
    }
    const double mem = residentMiB() - before;

    const auto start = Clock::now();
    for (int frame = 0; frame < frames; ++frame) {
        for (auto& e : entities) {
            auto p = e->getComponent<LegacyPosition>();
            auto v = e->getComponent<LegacyVelocity>();
            p->x += v->x * kDt;
            p->y += v->y * kDt;
            p->z += v->z * kDt;
        }
    }
    const double iterMs = msSince(start);

    double checksum = 0.0;
    for (auto& e : entities) checksum += e->getComponent<LegacyPosition>()->x;
    return {iterMs, mem, checksum};
}

Result benchmarkArchetype(std::size_t count, int frames) {
    using namespace IKore::ecs;
    trimHeap();
    const double before = residentMiB();
    Registry world;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = world.create();
        world.add<Position>(e, Position{});
        world.add<Velocity>(e, Velocity{});
    }
    const double mem = residentMiB() - before;

    const auto start = Clock::now();
    for (int frame = 0; frame < frames; ++frame) {
        world.view<Position, Velocity>().each([](Entity, Position& p, Velocity& v) {
            p.x += v.x * kDt;
            p.y += v.y * kDt;
            p.z += v.z * kDt;
        });
    }
    const double iterMs = msSince(start);

    double checksum = 0.0;
    world.view<Position>().each([&checksum](Entity, Position& p) { checksum += p.x; });
    return {iterMs, mem, checksum};
}

// Returns the iteration speedup (legacy / archetype) for this size.
double runSize(std::size_t count, int frames) {
    const Result legacy = benchmarkLegacy(count, frames);
    const Result archetype = benchmarkArchetype(count, frames);

    const double updates = static_cast<double>(count) * frames;
    const double legacyNsPer = legacy.iterMs * 1e6 / updates;
    const double archNsPer = archetype.iterMs * 1e6 / updates;
    const double speedup = legacy.iterMs / archetype.iterMs;

    std::printf("\nEntities: %zu  |  Frames: %d  |  Updates: %.0f\n", count, frames, updates);
    std::printf("  %-12s %12s %12s %12s\n", "model", "iter (ms)", "ns/update", "mem (MiB)");
    std::printf("  %-12s %12.1f %12.2f %12.1f\n", "legacy", legacy.iterMs, legacyNsPer, legacy.memMiB);
    std::printf("  %-12s %12.1f %12.2f %12.1f\n", "archetype", archetype.iterMs, archNsPer,
                archetype.memMiB);
    std::printf("  -> iteration speedup: %.1fx", speedup);
    if (legacy.memMiB > 0.0 && archetype.memMiB > 0.0) {
        std::printf("   |   memory: %.1fx less\n", legacy.memMiB / archetype.memMiB);
    } else {
        std::printf("\n");
    }
    // Guard against the optimizer discarding the work (checksums should match).
    std::printf("  (checksum legacy=%.3f archetype=%.3f)\n", legacy.checksum, archetype.checksum);
    return speedup;
}

} // namespace

int main(int argc, char** argv) {
    std::printf("========================================\n");
    std::printf("   ECS storage benchmark (legacy vs archetype)\n");
    std::printf("   Milestone 8 / #143\n");
    std::printf("========================================\n");

    int frames = 50;
    if (argc > 1) frames = std::atoi(argv[1]);

    const double speedupSmall = runSize(100000, frames);
    const double speedupLarge = runSize(1000000, frames);

    std::printf("\nNote: absolute timings are machine-dependent; the speedup ratio is the\n");
    std::printf("headline. Build with -O2 for representative numbers.\n");

    // Doubles as a regression guard: the archetype storage must iterate faster.
    if (speedupSmall <= 1.0 || speedupLarge <= 1.0) {
        std::printf("\nREGRESSION: archetype storage was not faster than the legacy model.\n");
        return 1;
    }
    return 0;
}
