// Test for the entity inspector core - Milestone 9, #56.
//
// Two surfaces:
//   1. The generic reflection + selection model (EntityInspector): typed
//      get/set properties round-trip, and the selection cache rebuilds only when
//      the selection changes (the "no per-frame allocation" guarantee).
//   2. The archetype-ECS bridge (EcsInspector): real Transform/Velocity/RigidBody/
//      AIAgent components reflect with correct values and edits apply live to the
//      running Registry.
//
// Pure std + header-only (the ECS is header-only and glm-free):
//   g++ -std=c++17 -I src tests/test_entity_inspector.cpp -o test_entity_inspector

#include "ui/EcsInspector.h"
#include "ui/EntityInspector.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
namespace ecs = IKore::ecs;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static const Property* findProp(const ComponentView& cv, const std::string& name) {
    for (const Property& p : cv.properties) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

static const ComponentView* findView(const std::vector<ComponentView>& views, const std::string& name) {
    for (const ComponentView& v : views) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

// ---- 1. Generic reflection + selection cache ------------------------------

// Stands in for an arbitrary object with one field of each property type.
struct Demo {
    bool active{true};
    int count{3};
    float weight{2.5f};
    Vec3f pos{1.0f, 2.0f, 3.0f};
    std::string label{"hi"};
};

static ComponentView buildDemoView(Demo& d) {
    ComponentView cv;
    cv.name = "Demo";
    cv.properties.push_back(propBool("active", [&d] { return d.active; },
                                     [&d](bool b) { d.active = b; }));
    cv.properties.push_back(propInt("count", [&d] { return d.count; }, [&d](int i) { d.count = i; }));
    cv.properties.push_back(propFloat("weight", [&d] { return d.weight; },
                                      [&d](float f) { d.weight = f; }));
    cv.properties.push_back(propFloat3("pos", [&d] { return d.pos; }, [&d](Vec3f v) { d.pos = v; }));
    cv.properties.push_back(propString("label", [&d] { return d.label; },
                                       [&d](std::string s) { d.label = std::move(s); }));
    return cv;
}

static void testGenericReflectionRoundTrip() {
    Demo demo;
    EntityInspector inspector;
    inspector.setBuilder([&demo](EntityInspector::EntityId) {
        return std::vector<ComponentView>{buildDemoView(demo)};
    });

    inspector.select(1);
    CHECK(inspector.hasSelection());
    CHECK(inspector.selected() == 1u);
    CHECK(inspector.components().size() == 1);

    const ComponentView& cv = inspector.components().front();
    CHECK(cv.name == "Demo");
    CHECK(cv.properties.size() == 5);

    // Getters read the live values.
    CHECK(findProp(cv, "active")->getBool() == true);
    CHECK(findProp(cv, "count")->getInt() == 3);
    CHECK(approx(findProp(cv, "weight")->getFloat(), 2.5f));
    CHECK(approx(findProp(cv, "pos")->getVec3().y, 2.0f));
    CHECK(findProp(cv, "label")->getString() == "hi");

    // Setters write straight back into the object (edits apply live).
    findProp(cv, "active")->setBool(false);
    findProp(cv, "count")->setInt(42);
    findProp(cv, "weight")->setFloat(9.75f);
    findProp(cv, "pos")->setVec3(Vec3f{7.0f, 8.0f, 9.0f});
    findProp(cv, "label")->setString("edited");

    CHECK(demo.active == false);
    CHECK(demo.count == 42);
    CHECK(approx(demo.weight, 9.75f));
    CHECK(approx(demo.pos.x, 7.0f) && approx(demo.pos.z, 9.0f));
    CHECK(demo.label == "edited");
}

static void testSelectionCacheDoesNotRebuild() {
    Demo demo;
    EntityInspector inspector;
    inspector.setBuilder([&demo](EntityInspector::EntityId) {
        return std::vector<ComponentView>{buildDemoView(demo)};
    });

    inspector.select(1);
    CHECK(inspector.rebuildCount() == 1);

    // Re-selecting the same entity and reading components must not rebuild - this
    // is what keeps the panel allocation-free per frame.
    inspector.select(1);
    for (int i = 0; i < 100; ++i) {
        (void)inspector.components();
        findProp(inspector.components().front(), "weight")->setFloat(static_cast<float>(i));
    }
    CHECK(inspector.rebuildCount() == 1);

    // A different entity, and an explicit refresh, do rebuild.
    inspector.select(2);
    CHECK(inspector.rebuildCount() == 2);
    inspector.refresh();
    CHECK(inspector.rebuildCount() == 3);

    inspector.deselect();
    CHECK(!inspector.hasSelection());
    CHECK(inspector.components().empty());
    CHECK(inspector.rebuildCount() == 3); // deselect clears without rebuilding
}

// ---- 2. Archetype-ECS bridge ----------------------------------------------

static void testEntityIdPacking() {
    ecs::Entity e{7u, 3u};
    const EntityInspector::EntityId id = packEntity(e);
    const ecs::Entity back = unpackEntity(id);
    CHECK(back == e);
    CHECK(back.index == 7u && back.generation == 3u);
}

static void testEcsReflectionValuesAndLiveEdits() {
    ecs::Registry reg;
    ecs::Entity e = reg.create();
    reg.add<ecs::Transform>(e, ecs::Transform{{1.0f, 2.0f, 3.0f}, {0.1f, 0.2f, 0.3f}, {2.0f, 2.0f, 2.0f}});
    reg.add<ecs::Velocity>(e, ecs::Velocity{{5.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});

    const std::vector<ComponentView> views = reflectEntity(reg, e);
    CHECK(views.size() == 2);

    const ComponentView* transform = findView(views, "Transform");
    CHECK(transform != nullptr);
    const ComponentView* velocity = findView(views, "Velocity");
    CHECK(velocity != nullptr);
    if (transform == nullptr || velocity == nullptr) return;

    // Values reflect the live component.
    const Vec3f pos = findProp(*transform, "position")->getVec3();
    CHECK(approx(pos.x, 1.0f) && approx(pos.y, 2.0f) && approx(pos.z, 3.0f));
    const Vec3f scale = findProp(*transform, "scale")->getVec3();
    CHECK(approx(scale.x, 2.0f));
    CHECK(approx(findProp(*velocity, "linear")->getVec3().x, 5.0f));

    // Editing a property writes straight into the ECS component.
    findProp(*transform, "position")->setVec3(Vec3f{10.0f, 20.0f, 30.0f});
    CHECK(approx(reg.get<ecs::Transform>(e).position.x, 10.0f));
    CHECK(approx(reg.get<ecs::Transform>(e).position.z, 30.0f));

    findProp(*velocity, "angular")->setVec3(Vec3f{0.0f, 0.0f, 9.0f});
    CHECK(approx(reg.get<ecs::Velocity>(e).angular.z, 9.0f));
}

static void testEcsScalarAndBoolFields() {
    ecs::Registry reg;
    ecs::Entity e = reg.create();
    reg.add<ecs::RigidBody>(e, ecs::RigidBody{{0.0f, -9.8f, 0.0f}, 2.0f, 0.1f, false});
    reg.add<ecs::AIAgent>(e, ecs::AIAgent{{1.0f, 0.0f, 0.0f}, 3.0f, 0.5f, true});

    const std::vector<ComponentView> views = reflectEntity(reg, e);
    const ComponentView* body = findView(views, "RigidBody");
    const ComponentView* ai = findView(views, "AIAgent");
    CHECK(body != nullptr && ai != nullptr);
    if (body == nullptr || ai == nullptr) return;

    CHECK(approx(findProp(*body, "mass")->getFloat(), 2.0f));
    CHECK(findProp(*body, "kinematic")->getBool() == false);

    findProp(*body, "mass")->setFloat(5.5f);
    findProp(*body, "kinematic")->setBool(true);
    CHECK(approx(reg.get<ecs::RigidBody>(e).mass, 5.5f));
    CHECK(reg.get<ecs::RigidBody>(e).kinematic == true);

    CHECK(findProp(*ai, "active")->getBool() == true);
    findProp(*ai, "speed")->setFloat(8.0f);
    findProp(*ai, "active")->setBool(false);
    CHECK(approx(reg.get<ecs::AIAgent>(e).speed, 8.0f));
    CHECK(reg.get<ecs::AIAgent>(e).active == false);
}

static void testEcsReflectsOnlyPresentComponents() {
    ecs::Registry reg;
    ecs::Entity bare = reg.create(); // no components
    CHECK(reflectEntity(reg, bare).empty());

    ecs::Entity onlyTransform = reg.create();
    reg.add<ecs::Transform>(onlyTransform, ecs::Transform{});
    const std::vector<ComponentView> views = reflectEntity(reg, onlyTransform);
    CHECK(views.size() == 1);
    CHECK(findView(views, "Transform") != nullptr);
    CHECK(findView(views, "Velocity") == nullptr);

    // Invalid handles reflect to nothing rather than crashing.
    CHECK(reflectEntity(reg, ecs::Entity::invalid()).empty());
}

static void testInspectorDrivenByEcsBuilder() {
    ecs::Registry reg;
    ecs::Entity a = reg.create();
    reg.add<ecs::Transform>(a, ecs::Transform{{4.0f, 5.0f, 6.0f}, {}, {1.0f, 1.0f, 1.0f}});
    ecs::Entity b = reg.create();
    reg.add<ecs::Transform>(b, ecs::Transform{});
    reg.add<ecs::AIAgent>(b, ecs::AIAgent{});

    EntityInspector inspector;
    installEcsBuilder(inspector, reg);

    inspector.select(packEntity(a));
    CHECK(inspector.hasSelection());
    CHECK(inspector.components().size() == 1); // Transform only
    const ComponentView& ta = inspector.components().front();
    CHECK(approx(findProp(ta, "position")->getVec3().x, 4.0f));
    // Edit through the cached property reaches the live component.
    findProp(ta, "position")->setVec3(Vec3f{40.0f, 50.0f, 60.0f});
    CHECK(approx(reg.get<ecs::Transform>(a).position.y, 50.0f));

    // Switching selection reflects a different component set.
    inspector.select(packEntity(b));
    CHECK(inspector.components().size() == 2); // Transform + AIAgent
    CHECK(findView(inspector.components(), "AIAgent") != nullptr);
}

int main() {
    testGenericReflectionRoundTrip();
    testSelectionCacheDoesNotRebuild();
    testEntityIdPacking();
    testEcsReflectionValuesAndLiveEdits();
    testEcsScalarAndBoolFields();
    testEcsReflectsOnlyPresentComponents();
    testInspectorDrivenByEcsBuilder();

    if (g_failures == 0) {
        std::printf("All entity inspector tests passed.\n");
        return 0;
    }
    std::printf("%d entity inspector test(s) failed.\n", g_failures);
    return 1;
}
