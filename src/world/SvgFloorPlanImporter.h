#pragma once

#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

/**
 * @file SvgFloorPlanImporter.h
 * @brief Import a 2D SVG floor plan into 3D walls/rooms (issue #148).
 *
 * Milestone 11 - the first World-from-Data importer. It parses a focused subset
 * of SVG used by floor plans - <line>, <rect>, <polyline>, <polygon>, and <path>
 * (M/L/H/V/Z, absolute + relative) - turns each wall segment into an extruded 3D
 * box, and produces a renderer-agnostic FloorPlan. Elements whose `class`
 * contains "door" or "opening" are treated as openings (collected separately,
 * not built as solid walls), so doorways appear where the source layers indicate.
 *
 * SVG is 2D (x, y); it maps onto the world XZ plane (x -> x, y -> z) with walls
 * extruded up +Y and a floor slab generated under the bounding box.
 *
 * Output is renderer-agnostic (a list of oriented boxes). emitToRegistry() shows
 * the engine wiring: one entity per wall (and the floor) with a Transform whose
 * scale is the box size and whose rotation.y is the wall's yaw - so the renderer
 * just draws a unit cube per entity. Pure std + the header-only ECS, so it builds
 * and is unit-tested in isolation; hooking it to the live scene graph/GL renderer
 * is the follow-on.
 */
namespace IKore {
namespace world {

/// An oriented box: a unit cube scaled by @c size, rotated @c yaw about Y, at @c center.
struct Box {
    ecs::Vec3 center{};
    ecs::Vec3 size{1.0f, 1.0f, 1.0f};
    float yaw{0.0f}; // radians, about the Y axis
};

struct FloorPlan {
    std::vector<Box> walls;
    std::vector<Box> doors; // openings indicated by the source layers
    Box floor{};
    bool hasFloor{false};
};

struct ImportOptions {
    float scale{1.0f};          // SVG units -> world units
    float wallHeight{3.0f};
    float wallThickness{0.2f};
    float floorThickness{0.1f};
    bool generateFloor{true};
};

namespace detail {

struct Vec2 {
    double x{0.0}, y{0.0};
};
struct Segment {
    Vec2 a{}, b{};
    bool isDoor{false};
};

/// Value of attribute @p name in an element's attribute text, or "" if absent.
inline std::string attr(const std::string& attrs, const std::string& name) {
    std::size_t pos = 0;
    while ((pos = attrs.find(name, pos)) != std::string::npos) {
        // Require the match to be a whole attribute name (preceded by start/space,
        // followed by optional spaces then '=').
        const bool boundaryBefore = (pos == 0) || std::isspace(static_cast<unsigned char>(attrs[pos - 1]));
        std::size_t after = pos + name.size();
        std::size_t eq = after;
        while (eq < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[eq]))) ++eq;
        if (boundaryBefore && eq < attrs.size() && attrs[eq] == '=') {
            std::size_t q = eq + 1;
            while (q < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[q]))) ++q;
            if (q < attrs.size() && (attrs[q] == '"' || attrs[q] == '\'')) {
                const char quote = attrs[q];
                const std::size_t start = q + 1;
                const std::size_t end = attrs.find(quote, start);
                if (end != std::string::npos) return attrs.substr(start, end - start);
            }
        }
        pos = after;
    }
    return std::string();
}

inline double toDouble(const std::string& s, double fallback = 0.0) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    return end == s.c_str() ? fallback : v;
}

inline bool isDoorClass(const std::string& attrs) {
    const std::string cls = attr(attrs, "class") + " " + attr(attrs, "id");
    return cls.find("door") != std::string::npos || cls.find("opening") != std::string::npos;
}

/// Parse a "x,y x,y ..." (or space/comma separated) list of points.
inline std::vector<Vec2> parsePoints(const std::string& s) {
    std::vector<Vec2> pts;
    std::vector<double> nums;
    std::size_t i = 0;
    while (i < s.size()) {
        if (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',') {
            ++i;
            continue;
        }
        char* end = nullptr;
        const double v = std::strtod(s.c_str() + i, &end);
        if (end == s.c_str() + i) {
            ++i;
            continue;
        }
        nums.push_back(v);
        i = static_cast<std::size_t>(end - s.c_str());
    }
    for (std::size_t k = 0; k + 1 < nums.size(); k += 2) pts.push_back({nums[k], nums[k + 1]});
    return pts;
}

/// Parse an SVG path "d" attribute (M/L/H/V/Z, absolute + relative) into segments.
inline void parsePath(const std::string& d, bool isDoor, std::vector<Segment>& out) {
    std::size_t i = 0;
    auto skipSep = [&] {
        while (i < d.size() && (std::isspace(static_cast<unsigned char>(d[i])) || d[i] == ',')) ++i;
    };
    auto readNum = [&](double& v) -> bool {
        skipSep();
        if (i >= d.size()) return false;
        char* end = nullptr;
        const double parsed = std::strtod(d.c_str() + i, &end);
        if (end == d.c_str() + i) return false;
        v = parsed;
        i = static_cast<std::size_t>(end - d.c_str());
        return true;
    };

    Vec2 cur{}, start{};
    bool haveStart = false;
    char cmd = 0;
    while (i < d.size()) {
        skipSep();
        if (i >= d.size()) break;
        if (std::isalpha(static_cast<unsigned char>(d[i]))) {
            cmd = d[i++];
        }
        const bool rel = (cmd >= 'a' && cmd <= 'z');
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
        if (c == 'M') {
            double x, y;
            if (!readNum(x) || !readNum(y)) break;
            if (rel && haveStart) { x += cur.x; y += cur.y; }
            cur = {x, y};
            start = cur;
            haveStart = true;
            cmd = rel ? 'l' : 'L'; // subsequent implicit pairs are line-tos
        } else if (c == 'L') {
            double x, y;
            if (!readNum(x) || !readNum(y)) break;
            Vec2 next = rel ? Vec2{cur.x + x, cur.y + y} : Vec2{x, y};
            out.push_back({cur, next, isDoor});
            cur = next;
        } else if (c == 'H') {
            double x;
            if (!readNum(x)) break;
            Vec2 next = rel ? Vec2{cur.x + x, cur.y} : Vec2{x, cur.y};
            out.push_back({cur, next, isDoor});
            cur = next;
        } else if (c == 'V') {
            double y;
            if (!readNum(y)) break;
            Vec2 next = rel ? Vec2{cur.x, cur.y + y} : Vec2{cur.x, y};
            out.push_back({cur, next, isDoor});
            cur = next;
        } else if (c == 'Z') {
            if (haveStart) {
                out.push_back({cur, start, isDoor});
                cur = start;
            }
        } else {
            // Unsupported command: stop to avoid misparsing.
            break;
        }
    }
}

/// Extract wall/door segments from the supported SVG elements.
inline std::vector<Segment> extractSegments(const std::string& svg) {
    std::vector<Segment> segs;
    std::size_t i = 0;
    while (i < svg.size()) {
        const std::size_t lt = svg.find('<', i);
        if (lt == std::string::npos) break;
        if (svg.compare(lt, 4, "<!--") == 0) { // skip comments
            const std::size_t endc = svg.find("-->", lt + 4);
            i = (endc == std::string::npos) ? svg.size() : endc + 3;
            continue;
        }
        std::size_t p = lt + 1;
        if (p < svg.size() && (svg[p] == '/' || svg[p] == '!' || svg[p] == '?')) {
            const std::size_t gt = svg.find('>', p);
            i = (gt == std::string::npos) ? svg.size() : gt + 1;
            continue;
        }
        std::size_t nameEnd = p;
        while (nameEnd < svg.size() && (std::isalnum(static_cast<unsigned char>(svg[nameEnd])) || svg[nameEnd] == '_'))
            ++nameEnd;
        const std::string name = svg.substr(p, nameEnd - p);
        const std::size_t gt = svg.find('>', nameEnd);
        if (gt == std::string::npos) break;
        std::string attrs = svg.substr(nameEnd, gt - nameEnd);
        if (!attrs.empty() && attrs.back() == '/') attrs.pop_back();
        i = gt + 1;

        const bool door = isDoorClass(attrs);
        if (name == "line") {
            segs.push_back({{toDouble(attr(attrs, "x1")), toDouble(attr(attrs, "y1"))},
                            {toDouble(attr(attrs, "x2")), toDouble(attr(attrs, "y2"))},
                            door});
        } else if (name == "rect") {
            const double x = toDouble(attr(attrs, "x")), y = toDouble(attr(attrs, "y"));
            const double w = toDouble(attr(attrs, "width")), h = toDouble(attr(attrs, "height"));
            const Vec2 a{x, y}, b{x + w, y}, c{x + w, y + h}, dd{x, y + h};
            segs.push_back({a, b, door});
            segs.push_back({b, c, door});
            segs.push_back({c, dd, door});
            segs.push_back({dd, a, door});
        } else if (name == "polyline" || name == "polygon") {
            const std::vector<Vec2> pts = parsePoints(attr(attrs, "points"));
            for (std::size_t k = 0; k + 1 < pts.size(); ++k) segs.push_back({pts[k], pts[k + 1], door});
            if (name == "polygon" && pts.size() > 2) segs.push_back({pts.back(), pts.front(), door});
        } else if (name == "path") {
            parsePath(attr(attrs, "d"), door, segs);
        }
    }
    return segs;
}

} // namespace detail

/// Parse an SVG floor plan (as text) into a renderer-agnostic FloorPlan.
inline FloorPlan importString(const std::string& svg, const ImportOptions& options = {}) {
    using detail::Segment;
    using detail::Vec2;

    const std::vector<Segment> segments = detail::extractSegments(svg);
    FloorPlan plan;

    const float s = options.scale;
    const float halfH = options.wallHeight * 0.5f;
    bool haveBounds = false;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;

    auto accountBounds = [&](const Vec2& p) {
        if (!haveBounds) {
            minX = maxX = p.x;
            minY = maxY = p.y;
            haveBounds = true;
        } else {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    };

    for (const Segment& seg : segments) {
        accountBounds(seg.a);
        accountBounds(seg.b);

        const double dx = (seg.b.x - seg.a.x);
        const double dy = (seg.b.y - seg.a.y);
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0) continue;

        Box box;
        box.center = ecs::Vec3{static_cast<float>((seg.a.x + seg.b.x) * 0.5) * s, halfH,
                               static_cast<float>((seg.a.y + seg.b.y) * 0.5) * s};
        box.size = ecs::Vec3{static_cast<float>(length) * s, options.wallHeight, options.wallThickness};
        box.yaw = static_cast<float>(std::atan2(dy, dx)); // SVG y maps to world z
        (seg.isDoor ? plan.doors : plan.walls).push_back(box);
    }

    if (options.generateFloor && haveBounds) {
        const double w = maxX - minX;
        const double d = maxY - minY;
        plan.floor.center = ecs::Vec3{static_cast<float>((minX + maxX) * 0.5) * s,
                                      -options.floorThickness * 0.5f,
                                      static_cast<float>((minY + maxY) * 0.5) * s};
        plan.floor.size = ecs::Vec3{static_cast<float>(w) * s, options.floorThickness, static_cast<float>(d) * s};
        plan.hasFloor = true;
    }

    return plan;
}

/**
 * @brief Emit a FloorPlan into an ecs::Registry: one entity per wall (and the
 *        floor) with a Transform (scale = box size, rotation.y = yaw).
 * @return the number of entities created.
 */
inline std::size_t emitToRegistry(const FloorPlan& plan, ecs::Registry& registry) {
    std::size_t created = 0;
    auto emitBox = [&](const Box& b) {
        ecs::Entity e = registry.create();
        registry.add<ecs::Transform>(e, ecs::Transform{b.center, ecs::Vec3{0.0f, b.yaw, 0.0f}, b.size});
        ++created;
    };
    for (const Box& wall : plan.walls) emitBox(wall);
    if (plan.hasFloor) emitBox(plan.floor);
    return created;
}

} // namespace world
} // namespace IKore
