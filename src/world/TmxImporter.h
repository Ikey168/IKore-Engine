#pragma once

#include "core/ecs/Registry.h"
#include "core/ecs/components/Components.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

/**
 * @file TmxImporter.h
 * @brief Import a Tiled (.tmx) tilemap into a 3D level (issue #149).
 *
 * Milestone 11 - the second World-from-Data importer. Parses an orthogonal Tiled
 * map: tile layers (CSV-encoded `<data>`) become a grid of 3D tile cells, and
 * object layers become entity spawns at their positions. Tile GIDs are preserved
 * (flip flags masked off) so a game can map each GID to a prefab; object name /
 * type / gid are preserved so spawns can be resolved to game objects.
 *
 * Tiled is 2D in pixels (x right, y down); this maps onto the world XZ plane
 * (x -> x, y -> z), tiles extruded up +Y. Output is renderer-agnostic;
 * emitToRegistry() creates one ecs entity per tile and per object with a
 * Transform (scale = cell/object size, position = world center).
 *
 * Supports CSV tile data (the common Tiled default). Base64/compressed layers are
 * skipped. Pure std + the header-only ECS, so it is unit-tested in isolation;
 * resolving GIDs to real prefab meshes and rendering them is the follow-on.
 */
namespace IKore {
namespace world {

struct TileInstance {
    std::uint32_t gid{0};   // Tiled global tile id (flip flags removed)
    int col{0}, row{0};     // grid coordinates
    ecs::Vec3 center{};
    ecs::Vec3 size{1.0f, 1.0f, 1.0f};
};

struct ObjectInstance {
    std::string name;
    std::string type;
    std::uint32_t gid{0};   // 0 for shape/point objects
    ecs::Vec3 center{};
    ecs::Vec3 size{};
};

struct TileLevel {
    int width{0}, height{0};       // in tiles
    int tileWidth{0}, tileHeight{0}; // in pixels
    std::vector<TileInstance> tiles;
    std::vector<ObjectInstance> objects;
};

struct TmxImportOptions {
    float scale{1.0f};            // pixels -> world units
    float tileWorldHeight{1.0f};  // extrusion height of a tile cell
};

namespace detail {

inline std::string attr(const std::string& attrs, const std::string& name) {
    std::size_t pos = 0;
    while ((pos = attrs.find(name, pos)) != std::string::npos) {
        const bool boundaryBefore = (pos == 0) || std::isspace(static_cast<unsigned char>(attrs[pos - 1]));
        std::size_t eq = pos + name.size();
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
        pos += name.size();
    }
    return std::string();
}

inline int toInt(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;
    return static_cast<int>(std::strtol(s.c_str(), nullptr, 10));
}
inline double toDouble(const std::string& s, double fallback = 0.0) {
    if (s.empty()) return fallback;
    return std::strtod(s.c_str(), nullptr);
}

// Read the attribute text of the tag starting at `tagStart` ('<'); returns the
// attrs (between tag name and '>') and sets `posAfterGt` to just past '>'.
inline std::string readTag(const std::string& xml, std::size_t tagStart, std::size_t& posAfterGt) {
    const std::size_t gt = xml.find('>', tagStart);
    if (gt == std::string::npos) {
        posAfterGt = xml.size();
        return std::string();
    }
    posAfterGt = gt + 1;
    std::size_t nameEnd = tagStart + 1;
    while (nameEnd < gt && !std::isspace(static_cast<unsigned char>(xml[nameEnd]))) ++nameEnd;
    std::string attrs = xml.substr(nameEnd, gt - nameEnd);
    if (!attrs.empty() && attrs.back() == '/') attrs.pop_back();
    return attrs;
}

} // namespace detail

inline TileLevel importString(const std::string& tmx, const TmxImportOptions& options = {}) {
    using namespace detail;
    TileLevel level;

    // --- map attributes ---
    std::size_t mapPos = tmx.find("<map");
    if (mapPos != std::string::npos) {
        std::size_t after = 0;
        const std::string a = readTag(tmx, mapPos, after);
        level.width = toInt(attr(a, "width"));
        level.height = toInt(attr(a, "height"));
        level.tileWidth = toInt(attr(a, "tilewidth"));
        level.tileHeight = toInt(attr(a, "tileheight"));
    }

    const float s = options.scale;
    const float tw = static_cast<float>(level.tileWidth) * s;
    const float th = static_cast<float>(level.tileHeight) * s;
    const float halfTileH = options.tileWorldHeight * 0.5f;

    // --- tile layers (CSV) ---
    std::size_t pos = 0;
    while ((pos = tmx.find("<layer", pos)) != std::string::npos) {
        std::size_t afterLayer = 0;
        const std::string layerAttrs = readTag(tmx, pos, afterLayer);
        const int lw = toInt(attr(layerAttrs, "width"), level.width);

        const std::size_t dataPos = tmx.find("<data", afterLayer);
        const std::size_t layerEnd = tmx.find("</layer>", afterLayer);
        if (dataPos == std::string::npos || (layerEnd != std::string::npos && dataPos > layerEnd)) {
            pos = afterLayer;
            continue;
        }
        std::size_t afterData = 0;
        const std::string dataAttrs = readTag(tmx, dataPos, afterData);
        const std::string encoding = attr(dataAttrs, "encoding");
        const std::size_t dataEnd = tmx.find("</data>", afterData);
        if (encoding.find("csv") != std::string::npos && dataEnd != std::string::npos) {
            const std::string csv = tmx.substr(afterData, dataEnd - afterData);
            int index = 0;
            std::size_t i = 0;
            while (i < csv.size()) {
                if (!(std::isdigit(static_cast<unsigned char>(csv[i])))) {
                    ++i;
                    continue;
                }
                char* end = nullptr;
                const unsigned long raw = std::strtoul(csv.c_str() + i, &end, 10);
                i = static_cast<std::size_t>(end - csv.c_str());
                const std::uint32_t gid = static_cast<std::uint32_t>(raw) & 0x1FFFFFFFu; // strip flip flags
                const int col = (lw > 0) ? (index % lw) : 0;
                const int row = (lw > 0) ? (index / lw) : 0;
                ++index;
                if (gid == 0) continue; // empty cell
                TileInstance tile;
                tile.gid = gid;
                tile.col = col;
                tile.row = row;
                tile.center = ecs::Vec3{(static_cast<float>(col) + 0.5f) * tw, halfTileH,
                                        (static_cast<float>(row) + 0.5f) * th};
                tile.size = ecs::Vec3{tw, options.tileWorldHeight, th};
                level.tiles.push_back(tile);
            }
        }
        pos = (dataEnd == std::string::npos) ? afterData : dataEnd + 7;
    }

    // --- object layers: every <object ...> element ---
    pos = 0;
    while ((pos = tmx.find("<object", pos)) != std::string::npos) {
        const std::size_t nameCharPos = pos + 7; // char after "<object"
        const char nc = (nameCharPos < tmx.size()) ? tmx[nameCharPos] : '\0';
        if (!(std::isspace(static_cast<unsigned char>(nc)) || nc == '/' || nc == '>')) {
            pos = nameCharPos; // e.g. "<objectgroup" - not an object element
            continue;
        }
        std::size_t after = 0;
        const std::string a = readTag(tmx, pos, after);
        pos = after;

        const double x = toDouble(attr(a, "x"));
        const double y = toDouble(attr(a, "y"));
        const double w = toDouble(attr(a, "width"));
        const double h = toDouble(attr(a, "height"));
        ObjectInstance obj;
        obj.name = attr(a, "name");
        obj.type = attr(a, "type");
        obj.gid = static_cast<std::uint32_t>(std::strtoul(attr(a, "gid").c_str(), nullptr, 10)) & 0x1FFFFFFFu;
        obj.center = ecs::Vec3{static_cast<float>(x + w * 0.5) * s, 0.0f, static_cast<float>(y + h * 0.5) * s};
        obj.size = ecs::Vec3{static_cast<float>(w) * s, 0.0f, static_cast<float>(h) * s};
        level.objects.push_back(obj);
    }

    return level;
}

/// Emit one ecs entity (Transform) per tile and per object. Returns the count.
inline std::size_t emitToRegistry(const TileLevel& level, ecs::Registry& registry) {
    std::size_t created = 0;
    for (const TileInstance& tile : level.tiles) {
        ecs::Entity e = registry.create();
        registry.add<ecs::Transform>(e, ecs::Transform{tile.center, ecs::Vec3{}, tile.size});
        ++created;
    }
    for (const ObjectInstance& obj : level.objects) {
        ecs::Entity e = registry.create();
        registry.add<ecs::Transform>(e, ecs::Transform{obj.center, ecs::Vec3{}, obj.size});
        ++created;
    }
    return created;
}

} // namespace world
} // namespace IKore
