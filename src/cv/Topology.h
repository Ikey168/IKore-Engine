#pragma once

#include "cv/Vectorize.h" // Mask, Polyline, Point

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

/**
 * @file Topology.h
 * @brief Rooms, doorways, and a connectivity graph from walls (issue #168).
 *
 * Milestone 16. Derives dungeon topology from the vectorized wall lines (#167):
 *   - Rasterize the walls to a cell grid.
 *   - A doorway is a one-cell gap in a wall line: an open cell with walls on one
 *     opposite pair of sides and open on the other (a 1-wide passage).
 *   - Rooms are the connected components of open cells once doorway cells are
 *     treated as walls, so the two sides of a doorway are distinct rooms. A region
 *     touching the grid border is the "outside".
 *   - Each doorway links the two rooms it separates, forming the connectivity
 *     graph; gaps that do not cleanly join two rooms are flagged as ambiguous.
 *
 * Pure grid logic on the dependency-free Mask (no OpenCV), unit-testable headless.
 * Header-only.
 */
namespace IKore {
namespace cv {

struct Room {
    int id{-1};
    int area{0};
    bool isOutside{false};
    float cx{0.0f};
    float cy{0.0f};
};

struct Doorway {
    int x{0};
    int y{0};
    int roomA{-1};
    int roomB{-1};
    bool toOutside{false};
};

struct Topology {
    int width{0};
    int height{0};
    std::vector<int> label; ///< per cell: -1 = wall/doorway, else room id.
    std::vector<Room> rooms;
    std::vector<Doorway> doorways;
    std::vector<std::pair<int, int>> connections; ///< undirected room-id pairs.
    int ambiguousGaps{0};

    int labelAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return -1;
        return label[static_cast<std::size_t>(y) * width + x];
    }
    int interiorRoomCount() const {
        int n = 0;
        for (const Room& r : rooms) {
            if (!r.isOutside) ++n;
        }
        return n;
    }
    /// True if a doorway directly links rooms @p a and @p b.
    bool connected(int a, int b) const {
        const std::pair<int, int> key{std::min(a, b), std::max(a, b)};
        for (const std::pair<int, int>& c : connections) {
            if (c == key) return true;
        }
        return false;
    }
    /// True if rooms @p a and @p b are linked through any chain of doorways.
    bool reachable(int a, int b) const {
        if (a == b) return true;
        std::vector<std::vector<int>> adj(rooms.size());
        for (const std::pair<int, int>& c : connections) {
            adj[static_cast<std::size_t>(c.first)].push_back(c.second);
            adj[static_cast<std::size_t>(c.second)].push_back(c.first);
        }
        std::vector<char> seen(rooms.size(), 0);
        std::vector<int> stack{a};
        seen[static_cast<std::size_t>(a)] = 1;
        while (!stack.empty()) {
            const int cur = stack.back();
            stack.pop_back();
            if (cur == b) return true;
            for (int nb : adj[static_cast<std::size_t>(cur)]) {
                if (!seen[static_cast<std::size_t>(nb)]) {
                    seen[static_cast<std::size_t>(nb)] = 1;
                    stack.push_back(nb);
                }
            }
        }
        return false;
    }
};

/// Build the topology from a wall-occupancy grid (1 = wall cell).
inline Topology buildTopologyFromGrid(const Mask& walls) {
    const int W = walls.width, H = walls.height;
    auto isWall = [&](int x, int y) { return walls.get(x, y) != 0; }; // out of bounds -> open

    // 1. Doorway cells: a 1-wide gap in a wall line. Block them for segmentation.
    Mask blocked = walls;
    std::vector<std::pair<int, int>> doors;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (isWall(x, y)) continue;
            const bool nW = isWall(x, y - 1), sW = isWall(x, y + 1);
            const bool eW = isWall(x + 1, y), wW = isWall(x - 1, y);
            if ((nW && sW && !eW && !wW) || (eW && wW && !nW && !sW)) {
                doors.push_back({x, y});
                blocked.set(x, y, 1);
            }
        }
    }

    // 2. Flood-fill open cells into rooms.
    Topology t;
    t.width = W;
    t.height = H;
    t.label.assign(static_cast<std::size_t>(W) * H, -1);
    const int dx4[4] = {1, -1, 0, 0};
    const int dy4[4] = {0, 0, 1, -1};
    std::vector<int> stack;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (blocked.get(x, y) || t.label[static_cast<std::size_t>(y) * W + x] != -1) continue;
            const int id = static_cast<int>(t.rooms.size());
            int area = 0;
            bool border = false;
            double sx = 0, sy = 0;
            stack.clear();
            stack.push_back(y * W + x);
            t.label[static_cast<std::size_t>(y) * W + x] = id;
            while (!stack.empty()) {
                const int cell = stack.back();
                stack.pop_back();
                const int cxx = cell % W, cyy = cell / W;
                ++area;
                sx += cxx;
                sy += cyy;
                if (cxx == 0 || cyy == 0 || cxx == W - 1 || cyy == H - 1) border = true;
                for (int i = 0; i < 4; ++i) {
                    const int ax = cxx + dx4[i], ay = cyy + dy4[i];
                    if (ax < 0 || ay < 0 || ax >= W || ay >= H) continue;
                    if (blocked.get(ax, ay)) continue;
                    if (t.label[static_cast<std::size_t>(ay) * W + ax] != -1) continue;
                    t.label[static_cast<std::size_t>(ay) * W + ax] = id;
                    stack.push_back(ay * W + ax);
                }
            }
            Room r;
            r.id = id;
            r.area = area;
            r.isOutside = border;
            r.cx = static_cast<float>(sx / area);
            r.cy = static_cast<float>(sy / area);
            t.rooms.push_back(r);
        }
    }

    // 3. Link the rooms on either side of each doorway.
    std::set<std::pair<int, int>> conn;
    for (const std::pair<int, int>& d : doors) {
        const int x = d.first, y = d.second;
        const bool nW = isWall(x, y - 1), sW = isWall(x, y + 1);
        int a, b;
        if (nW && sW) { // vertical wall, gap opens east-west
            a = t.labelAt(x - 1, y);
            b = t.labelAt(x + 1, y);
        } else { // horizontal wall, gap opens north-south
            a = t.labelAt(x, y - 1);
            b = t.labelAt(x, y + 1);
        }
        if (a >= 0 && b >= 0 && a != b) {
            Doorway door;
            door.x = x;
            door.y = y;
            door.roomA = a;
            door.roomB = b;
            door.toOutside = t.rooms[static_cast<std::size_t>(a)].isOutside ||
                             t.rooms[static_cast<std::size_t>(b)].isOutside;
            t.doorways.push_back(door);
            conn.insert({std::min(a, b), std::max(a, b)});
        } else {
            ++t.ambiguousGaps; // gap that does not cleanly join two rooms
        }
    }
    t.connections.assign(conn.begin(), conn.end());
    return t;
}

/// Rasterize wall polylines to a cell grid at @p gridSize, padded by @p pad cells.
inline Mask rasterizeWalls(const std::vector<Polyline>& walls, float gridSize, int pad = 1) {
    int maxX = 0, maxY = 0;
    for (const Polyline& poly : walls) {
        for (const Point& p : poly) {
            maxX = std::max(maxX, static_cast<int>(std::lround(p.x / gridSize)));
            maxY = std::max(maxY, static_cast<int>(std::lround(p.y / gridSize)));
        }
    }
    Mask m(maxX + 1 + 2 * pad, maxY + 1 + 2 * pad);
    for (const Polyline& poly : walls) {
        for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
            int x0 = static_cast<int>(std::lround(poly[i].x / gridSize)) + pad;
            int y0 = static_cast<int>(std::lround(poly[i].y / gridSize)) + pad;
            const int x1 = static_cast<int>(std::lround(poly[i + 1].x / gridSize)) + pad;
            const int y1 = static_cast<int>(std::lround(poly[i + 1].y / gridSize)) + pad;
            // Bresenham line of wall cells.
            const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
            const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true) {
                m.set(x0, y0, 1);
                if (x0 == x1 && y0 == y1) break;
                const int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }
    }
    return m;
}

/// Build the topology directly from vectorized wall polylines.
inline Topology buildTopology(const std::vector<Polyline>& walls, float gridSize, int pad = 1) {
    return buildTopologyFromGrid(rasterizeWalls(walls, gridSize, pad));
}

} // namespace cv
} // namespace IKore
