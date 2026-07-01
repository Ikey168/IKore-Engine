#pragma once

#include "core/ecs/components/Components.h" // for ecs::Vec3

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

/**
 * @file Picking.h
 * @brief Ray-based viewport picking core (issue #57).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer-agnostic math
 * behind mouse picking: turn a cursor position into a world-space ray, intersect
 * that ray with entity bounding volumes (AABB / sphere), and pick the nearest hit.
 * A small Picker state machine tracks hover and selection and gates on whether the
 * UI wants the mouse, so a click over an ImGui panel is consumed by the UI and
 * never reaches the scene. The engine feeds the resulting selection to the entity
 * inspector (#56).
 *
 * Vectors reuse the glm-free ecs::Vec3, so the whole thing is unit-testable without
 * glm or a GL context. Matrices are column-major (matching glm), so the engine can
 * pass glm::value_ptr(glm::inverse(proj * view)) straight into screenToRay().
 * Header-only, std only.
 */
namespace IKore {
namespace pick {

using Vec3 = ecs::Vec3;
using Id = std::uint64_t;

/// A world-space ray. @c dir is expected to be normalized.
struct Ray {
    Vec3 origin;
    Vec3 dir;
};

/// An axis-aligned bounding box.
struct Aabb {
    Vec3 min;
    Vec3 max;

    static Aabb fromCenterHalf(Vec3 center, Vec3 half) {
        return Aabb{center - half, center + half};
    }
};

/// A column-major 4x4 matrix (same layout as glm::mat4).
struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
};

/// A scene item that can be picked: an id plus its world-space bounds.
struct Pickable {
    Id id;
    Aabb bounds;
};

/// Result of a pick query.
struct PickResult {
    bool hit{false};
    Id id{0};
    float distance{0.0f};
};

/// Unproject a clip-space point (NDC x,y,z with w=1) through an inverse
/// view-projection matrix, applying the perspective divide, to world space.
inline Vec3 unproject(const Mat4& invViewProj, float ndcX, float ndcY, float ndcZ) {
    const float* m = invViewProj.m;
    float x = m[0] * ndcX + m[4] * ndcY + m[8] * ndcZ + m[12];
    float y = m[1] * ndcX + m[5] * ndcY + m[9] * ndcZ + m[13];
    float z = m[2] * ndcX + m[6] * ndcY + m[10] * ndcZ + m[14];
    float w = m[3] * ndcX + m[7] * ndcY + m[11] * ndcZ + m[15];
    if (w != 0.0f) {
        x /= w;
        y /= w;
        z /= w;
    }
    return Vec3{x, y, z};
}

/**
 * @brief Build a world-space pick ray from a cursor position in pixels.
 * @param mouseX,mouseY     Cursor position, pixels, origin at the top-left.
 * @param viewportW,viewportH Viewport size in pixels.
 * @param invViewProj       Inverse of (projection * view).
 *
 * Unprojects the near and far plane points under the cursor and returns the ray
 * from near toward far. Assumes the OpenGL NDC z range [-1, 1].
 */
inline Ray screenToRay(float mouseX, float mouseY, float viewportW, float viewportH,
                       const Mat4& invViewProj) {
    const float ndcX = 2.0f * (mouseX / viewportW) - 1.0f;
    const float ndcY = 1.0f - 2.0f * (mouseY / viewportH); // screen y is top-down
    const Vec3 nearPoint = unproject(invViewProj, ndcX, ndcY, -1.0f);
    const Vec3 farPoint = unproject(invViewProj, ndcX, ndcY, 1.0f);
    return Ray{nearPoint, (farPoint - nearPoint).normalized()};
}

/**
 * @brief Ray vs AABB (slab method). Only hits in front of the origin count.
 * @param[out] tHit Distance along the ray to the entry point, if hit.
 * @return true if the ray hits the box at t >= 0.
 */
inline bool rayAabb(const Ray& ray, const Aabb& box, float& tHit) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    const float o[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float d[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const float lo[3] = {box.min.x, box.min.y, box.min.z};
    const float hi[3] = {box.max.x, box.max.y, box.max.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-8f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false; // parallel and outside the slab
        } else {
            const float inv = 1.0f / d[i];
            float t1 = (lo[i] - o[i]) * inv;
            float t2 = (hi[i] - o[i]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}

/**
 * @brief Ray vs sphere. Only hits in front of the origin count.
 * @param[out] tHit Distance along the ray to the nearest hit, if any.
 */
inline bool raySphere(const Ray& ray, Vec3 center, float radius, float& tHit) {
    const Vec3 oc = ray.origin - center;
    const float b = oc.x * ray.dir.x + oc.y * ray.dir.y + oc.z * ray.dir.z; // dot(oc, dir)
    const float c = oc.lengthSquared() - radius * radius;
    const float disc = b * b - c; // dir is normalized, so a == 1
    if (disc < 0.0f) return false;
    const float sq = std::sqrt(disc);
    float t = -b - sq;      // near root
    if (t < 0.0f) t = -b + sq; // origin inside / near root behind: try the far root
    if (t < 0.0f) return false;
    tHit = t;
    return true;
}

/// Pick the nearest item whose AABB the ray hits.
inline PickResult pickNearest(const Ray& ray, const std::vector<Pickable>& items) {
    PickResult best;
    float bestT = std::numeric_limits<float>::max();
    for (const Pickable& item : items) {
        float t = 0.0f;
        if (rayAabb(ray, item.bounds, t) && t < bestT) {
            bestT = t;
            best.hit = true;
            best.id = item.id;
            best.distance = t;
        }
    }
    return best;
}

/// Whether a viewport pointer event should reach the scene. When the UI wants the
/// mouse (ImGui io.WantCaptureMouse), clicks are consumed by the UI, not the scene.
inline bool sceneShouldReceiveMouse(bool imguiWantsMouse) { return !imguiWantsMouse; }

/**
 * @brief Hover + selection state driven by per-frame viewport interaction.
 *
 * Call update() each frame with the pick ray, the scene's pickables, whether the
 * UI wants the mouse, and whether a select click happened. When the UI wants the
 * mouse the scene is left untouched (hover clears, the click is ignored), so UI
 * clicks never fall through to picking. Otherwise the nearest hit becomes the
 * hover; a click commits it as the selection, and clicking empty space clears it.
 */
class Picker {
public:
    void update(const Ray& ray, const std::vector<Pickable>& items, bool pointerOverUI, bool clicked) {
        if (pointerOverUI) {
            m_hasHover = false; // UI consumed the pointer; leave the selection as-is
            m_hoverId = 0;
            return;
        }
        const PickResult result = pickNearest(ray, items);
        m_hasHover = result.hit;
        m_hoverId = result.id;
        if (clicked) {
            m_hasSelection = result.hit;
            m_selectedId = result.hit ? result.id : 0;
        }
    }

    bool hasHover() const { return m_hasHover; }
    Id hovered() const { return m_hoverId; }

    bool hasSelection() const { return m_hasSelection; }
    Id selected() const { return m_selectedId; }

    /// Selection can also be driven externally (e.g. from the scene hierarchy #59).
    void select(Id id) {
        m_hasSelection = true;
        m_selectedId = id;
    }
    void clearSelection() {
        m_hasSelection = false;
        m_selectedId = 0;
    }

private:
    bool m_hasHover{false};
    Id m_hoverId{0};
    bool m_hasSelection{false};
    Id m_selectedId{0};
};

} // namespace pick
} // namespace IKore
