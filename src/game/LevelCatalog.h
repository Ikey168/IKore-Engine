#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file LevelCatalog.h
 * @brief UGC: share codes, a browsable feed, and stars (issue #174).
 *
 * Milestone 17 (Mobile & UGC). The headless service core behind user-generated
 * content: publish a map and get a short share code, look a map up by code to
 * play it, award stars, and browse the feed by New / Top / Hot. A backend hosts
 * this in production; the logic is pure data so it is unit-testable off-device.
 *
 * The level payload is opaque to the catalog (the doodle-level JSON produced by
 * the converter library), so publishing and fetching stays decoupled from the
 * game: a fetched entry's JSON loads straight back into a playable level.
 *
 * Timestamps are caller-supplied (no wall clock here) so ordering is
 * deterministic. Header-only, std only.
 */
namespace IKore {
namespace game {

struct LevelEntry {
    std::string code;
    std::string author;
    std::string title;
    std::string levelJson; ///< opaque payload (the doodle-level JSON).
    std::int64_t publishedAt{0};
    int stars{0};
    int plays{0};
};

class LevelCatalog {
public:
    /// Publish a map; returns its unique share code.
    std::string publish(const std::string& author, const std::string& title,
                        const std::string& levelJson, std::int64_t publishedAt) {
        LevelEntry e;
        e.code = makeCode(m_entries.size());
        e.author = author;
        e.title = title;
        e.levelJson = levelJson;
        e.publishedAt = publishedAt;
        m_index[e.code] = m_entries.size();
        m_entries.push_back(std::move(e));
        return m_entries.back().code;
    }

    /// Look up a map by its share code (nullptr if unknown).
    const LevelEntry* getByCode(const std::string& code) const {
        auto it = m_index.find(code);
        return it == m_index.end() ? nullptr : &m_entries[it->second];
    }

    /// Award a star to a map. Returns false for an unknown code.
    bool star(const std::string& code) {
        auto it = m_index.find(code);
        if (it == m_index.end()) return false;
        ++m_entries[it->second].stars;
        return true;
    }

    /// Record that a map was played. Returns false for an unknown code.
    bool recordPlay(const std::string& code) {
        auto it = m_index.find(code);
        if (it == m_index.end()) return false;
        ++m_entries[it->second].plays;
        return true;
    }

    std::size_t size() const { return m_entries.size(); }

    /// Feed sorted newest first.
    std::vector<LevelEntry> browseNew(std::size_t limit = 50) const {
        return sorted(limit, [](const LevelEntry& a, const LevelEntry& b) {
            if (a.publishedAt != b.publishedAt) return a.publishedAt > b.publishedAt;
            return a.code > b.code;
        });
    }

    /// Feed sorted by most stars (newest breaks ties).
    std::vector<LevelEntry> browseTop(std::size_t limit = 50) const {
        return sorted(limit, [](const LevelEntry& a, const LevelEntry& b) {
            if (a.stars != b.stars) return a.stars > b.stars;
            return a.publishedAt > b.publishedAt;
        });
    }

    /// Feed sorted by a recency-weighted popularity score relative to @p now.
    std::vector<LevelEntry> browseHot(std::int64_t now, std::size_t limit = 50) const {
        const auto score = [now](const LevelEntry& e) {
            const double age = static_cast<double>(now - e.publishedAt);
            return (e.stars + 1.0) / (std::max(0.0, age) + 2.0);
        };
        return sorted(limit, [&score](const LevelEntry& a, const LevelEntry& b) {
            const double sa = score(a), sb = score(b);
            if (sa != sb) return sa > sb;
            return a.publishedAt > b.publishedAt;
        });
    }

    /// The rotating weekly drawing prompt for a given week index.
    static std::string weeklyPrompt(int weekIndex) {
        static const std::vector<std::string> prompts = {
            "Draw a castle keep", "Draw a twisting maze", "Draw a haunted manor",
            "Draw a treasure vault", "Draw a monster lair", "Draw an island fort",
        };
        const int n = static_cast<int>(prompts.size());
        int i = weekIndex % n;
        if (i < 0) i += n;
        return prompts[static_cast<std::size_t>(i)];
    }

private:
    static std::string makeCode(std::size_t id) {
        static const char* kDigits = "0123456789abcdefghijklmnopqrstuvwxyz";
        std::uint64_t v = static_cast<std::uint64_t>(id);
        std::string s;
        do {
            s += kDigits[v % 36];
            v /= 36;
        } while (v > 0);
        while (s.size() < 6) s += '0';
        std::reverse(s.begin(), s.end());
        return "DD-" + s;
    }

    template <class Less>
    std::vector<LevelEntry> sorted(std::size_t limit, Less less) const {
        std::vector<LevelEntry> out = m_entries;
        std::sort(out.begin(), out.end(), less);
        if (out.size() > limit) out.resize(limit);
        return out;
    }

    std::vector<LevelEntry> m_entries;
    std::unordered_map<std::string, std::size_t> m_index;
};

} // namespace game
} // namespace IKore
