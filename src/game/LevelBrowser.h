#pragma once

#include "game/LevelCatalog.h"
#include "game/LevelFormat.h" // fromLevelJson, LevelSpec

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file LevelBrowser.h
 * @brief Local level-browser view-model over the UGC catalog (issue #241).
 *
 * Milestone 17, "Doodlebound" Phase 5. The headless UI model a mobile shell
 * binds to for browsing published levels: pick a feed (New / Top / Hot), page
 * through the entries, move the selection, and get a preview of the highlighted
 * level - a small ASCII map plus counts (walls, coins, player/exit) so the list
 * shows a playable preview before you open it. play() records the play on the
 * catalog and hands back the level JSON to load into the game.
 *
 * The level payload stays the existing doodle-level JSON (parsed with
 * game::fromLevelJson) - no bespoke format. Timestamps are caller-supplied (the
 * catalog's Hot feed takes @c now), so the model is deterministic and testable
 * without a renderer or a wall clock. Header-only.
 */
namespace IKore {
namespace game {

/// Which ordering the browser shows.
enum class Feed { New, Top, Hot };

/// A compact, renderable summary of a level for the browser list.
struct LevelPreview {
    bool valid{false}; ///< false if the JSON failed to parse.
    std::string code;
    std::string title;
    std::string author;
    int wallCount{0};
    int coinCount{0};
    int enemyCount{0};
    bool hasPlayer{false};
    bool hasExit{false};
    int width{0};
    int height{0};
    std::string ascii; ///< small top-down map, rows separated by '\n'.

    /// A level is playable when it parsed and has both a spawn and an exit.
    bool playable() const { return valid && hasPlayer && hasExit; }
};

/// Render a preview (ASCII map + counts) of one catalog entry's level.
inline LevelPreview buildPreview(const LevelEntry& entry, int gridW = 24, int gridH = 12) {
    LevelPreview p;
    p.code = entry.code;
    p.title = entry.title;
    p.author = entry.author;
    if (gridW < 1) gridW = 1;
    if (gridH < 1) gridH = 1;
    p.width = gridW;
    p.height = gridH;

    LevelSpec spec;
    if (!fromLevelJson(entry.levelJson, spec)) {
        return p; // valid stays false
    }
    p.valid = true;
    p.wallCount = static_cast<int>(spec.walls.size());

    // World XZ bounds over every wall point and symbol position.
    float minX = 1e30f, maxX = -1e30f, minZ = 1e30f, maxZ = -1e30f;
    bool any = false;
    auto include = [&](float x, float z) {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
        any = true;
    };
    for (const Wall& w : spec.walls) {
        for (const ecs::Vec3& v : w.polyline) include(v.x, v.z);
    }
    for (const Symbol& s : spec.symbols) include(s.position.x, s.position.z);
    if (!any) {
        minX = maxX = minZ = maxZ = 0.0f;
    }
    const float spanX = maxX - minX > 1e-6f ? maxX - minX : 1.0f;
    const float spanZ = maxZ - minZ > 1e-6f ? maxZ - minZ : 1.0f;
    auto col = [&](float x) {
        int c = static_cast<int>(std::lround((x - minX) / spanX * (gridW - 1)));
        return std::max(0, std::min(gridW - 1, c));
    };
    auto row = [&](float z) {
        int r = static_cast<int>(std::lround((z - minZ) / spanZ * (gridH - 1)));
        return std::max(0, std::min(gridH - 1, r));
    };

    std::vector<char> grid(static_cast<std::size_t>(gridW) * gridH, ' ');
    auto put = [&](int cx, int cy, char ch) { grid[static_cast<std::size_t>(cy) * gridW + cx] = ch; };

    // Rasterize wall polylines (Bresenham on the preview grid).
    for (const Wall& w : spec.walls) {
        for (std::size_t i = 0; i + 1 < w.polyline.size(); ++i) {
            int x0 = col(w.polyline[i].x), y0 = row(w.polyline[i].z);
            const int x1 = col(w.polyline[i + 1].x), y1 = row(w.polyline[i + 1].z);
            const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
            const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true) {
                put(x0, y0, '#');
                if (x0 == x1 && y0 == y1) break;
                const int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }
    }

    // Place symbols (drawn over walls so spawns stay visible).
    for (const Symbol& s : spec.symbols) {
        const int cx = col(s.position.x), cy = row(s.position.z);
        char ch = '?';
        if (s.type == "player" || s.type == "start") { ch = 'P'; p.hasPlayer = true; }
        else if (s.type == "exit") { ch = 'E'; p.hasExit = true; }
        else if (s.type == "coin") { ch = 'o'; ++p.coinCount; }
        else if (s.type == "enemy") { ch = 'x'; ++p.enemyCount; }
        else if (s.type == "door" || s.type == "keydoor") { ch = 'D'; }
        put(cx, cy, ch);
    }

    p.ascii.reserve(static_cast<std::size_t>(gridW + 1) * gridH);
    for (int y = 0; y < gridH; ++y) {
        p.ascii.append(&grid[static_cast<std::size_t>(y) * gridW], static_cast<std::size_t>(gridW));
        p.ascii += '\n';
    }
    return p;
}

/// A browsable, selectable view over a LevelCatalog.
class LevelBrowser {
public:
    explicit LevelBrowser(LevelCatalog& catalog, std::int64_t now = 0, std::size_t limit = 50)
        : m_catalog(&catalog), m_now(now), m_limit(limit) {
        refresh();
    }

    void setFeed(Feed feed) {
        m_feed = feed;
        refresh();
    }
    Feed feed() const { return m_feed; }

    /// Update the reference time used by the Hot feed and re-sort if needed.
    void setNow(std::int64_t now) {
        m_now = now;
        if (m_feed == Feed::Hot) refresh();
    }

    /// Recompute the visible page from the catalog (call after the catalog changes).
    void refresh() {
        switch (m_feed) {
            case Feed::New: m_view = m_catalog->browseNew(m_limit); break;
            case Feed::Top: m_view = m_catalog->browseTop(m_limit); break;
            case Feed::Hot: m_view = m_catalog->browseHot(m_now, m_limit); break;
        }
        if (m_view.empty()) m_selected = 0;
        else if (m_selected >= m_view.size()) m_selected = m_view.size() - 1;
    }

    const std::vector<LevelEntry>& entries() const { return m_view; }
    std::size_t size() const { return m_view.size(); }
    bool empty() const { return m_view.empty(); }

    std::size_t selectedIndex() const { return m_selected; }
    const LevelEntry* selected() const { return m_view.empty() ? nullptr : &m_view[m_selected]; }

    /// Select an absolute index; false (selection unchanged) if out of range.
    bool select(std::size_t index) {
        if (index >= m_view.size()) return false;
        m_selected = index;
        return true;
    }

    /// Move the highlight by @p delta, clamped to the list bounds.
    void moveSelection(int delta) {
        if (m_view.empty()) return;
        long long i = static_cast<long long>(m_selected) + delta;
        if (i < 0) i = 0;
        if (i >= static_cast<long long>(m_view.size())) i = static_cast<long long>(m_view.size()) - 1;
        m_selected = static_cast<std::size_t>(i);
    }

    /// Preview of the highlighted entry (invalid preview if the list is empty).
    LevelPreview preview(int gridW = 24, int gridH = 12) const {
        const LevelEntry* e = selected();
        return e ? buildPreview(*e, gridW, gridH) : LevelPreview{};
    }

    /**
     * @brief Open the highlighted level: record a play on the catalog and hand back
     *        its JSON to load into the game. False (outJson untouched) if empty.
     */
    bool play(std::string& outJson) {
        const LevelEntry* e = selected();
        if (!e) return false;
        m_catalog->recordPlay(e->code);
        outJson = e->levelJson;
        return true;
    }

private:
    LevelCatalog* m_catalog;
    Feed m_feed{Feed::New};
    std::int64_t m_now{0};
    std::size_t m_limit{50};
    std::size_t m_selected{0};
    std::vector<LevelEntry> m_view;
};

} // namespace game
} // namespace IKore
