#pragma once

#include "cv/Symbols.h"      // cv::SymbolInstance
#include "cv/Vectorize.h"    // cv::Polyline
#include "game/DoodleScene.h" // Wall, Symbol, LevelSpec

#include <cstddef>
#include <string>
#include <vector>

/**
 * @file LevelReview.h
 * @brief Confidence + repair model for the interpreted level (issue #170).
 *
 * Milestone 16. Between the CV interpretation (walls #167, symbols #169) and play
 * (#162/#164), the user reviews the detected level and fixes misreads. This is the
 * headless, testable model behind that UI:
 *   - assembleLevel turns detected wall polylines and symbols into an editable
 *     InterpretedLevel; each symbol keeps its confidence and a needsReview flag.
 *   - Edit operations let the user correct a symbol's type, move / add / delete
 *     symbols, and edit walls.
 *   - issues() enumerates every problem (low-confidence marks not yet reviewed,
 *     missing start/exit, no walls) so nothing is dropped silently, and
 *     readyToPlay() gates play until they are resolved.
 *   - toLevelSpec() produces the LevelSpec for the M15 pipeline.
 *
 * Header-only; the renderer/ImGui layer draws editable icons over this model.
 */
namespace IKore {
namespace game {

/// A detected symbol the user can review and correct.
struct ReviewSymbol {
    std::string type;         ///< game object ("player","enemy","coin","exit",...).
    ecs::Vec3 position{};
    float confidence{1.0f};
    bool needsReview{false};  ///< low-confidence / ambiguous detection.
    bool reviewed{false};     ///< user confirmed or corrected it.
};

enum class IssueKind { LowConfidenceSymbol, NoPlayerStart, NoExit, NoWalls };

struct LevelIssue {
    IssueKind kind;
    int symbolIndex;     ///< -1 for level-wide issues.
    std::string message;
};

class InterpretedLevel {
public:
    std::vector<Wall> walls;
    std::vector<ReviewSymbol> symbols;
    float wallHeight{3.0f};
    float wallThickness{0.2f};

    // --- Edit operations (the "repair" the user performs) ------------------
    void setSymbolType(std::size_t i, const std::string& type) {
        if (i >= symbols.size()) return;
        symbols[i].type = type;
        symbols[i].reviewed = true; // user corrected it
        symbols[i].needsReview = false;
    }
    void markReviewed(std::size_t i) {
        if (i < symbols.size()) symbols[i].reviewed = true; // accept as detected
    }
    void moveSymbol(std::size_t i, const ecs::Vec3& pos) {
        if (i < symbols.size()) symbols[i].position = pos;
    }
    void deleteSymbol(std::size_t i) {
        if (i < symbols.size()) symbols.erase(symbols.begin() + static_cast<std::ptrdiff_t>(i));
    }
    void addSymbol(const std::string& type, const ecs::Vec3& pos) {
        ReviewSymbol s;
        s.type = type;
        s.position = pos;
        s.confidence = 1.0f;
        s.needsReview = false;
        s.reviewed = true; // user placed it deliberately
        symbols.push_back(s);
    }
    void addWall(const Wall& w) { walls.push_back(w); }
    void deleteWall(std::size_t i) {
        if (i < walls.size()) walls.erase(walls.begin() + static_cast<std::ptrdiff_t>(i));
    }

    bool hasSymbolType(const std::string& type) const {
        for (const ReviewSymbol& s : symbols) {
            if (s.type == type) return true;
        }
        return false;
    }

    // --- Review / validation ("never fail silently") -----------------------

    /// Every outstanding problem, so nothing is dropped without the user seeing it.
    std::vector<LevelIssue> issues() const {
        std::vector<LevelIssue> out;
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            if (symbols[i].needsReview && !symbols[i].reviewed) {
                out.push_back({IssueKind::LowConfidenceSymbol, static_cast<int>(i),
                               "low-confidence symbol '" + symbols[i].type + "' needs review"});
            }
        }
        if (walls.empty()) out.push_back({IssueKind::NoWalls, -1, "no walls detected"});
        if (!hasSymbolType("player")) {
            out.push_back({IssueKind::NoPlayerStart, -1, "no player start placed"});
        }
        if (!hasSymbolType("exit") && !hasSymbolType("door")) {
            out.push_back({IssueKind::NoExit, -1, "no exit placed"});
        }
        return out;
    }

    /// True only when there are no outstanding issues to review.
    bool readyToPlay() const { return issues().empty(); }

    /// Number of symbols still awaiting review.
    int pendingReviewCount() const {
        int n = 0;
        for (const ReviewSymbol& s : symbols) {
            if (s.needsReview && !s.reviewed) ++n;
        }
        return n;
    }

    /// Produce the LevelSpec for the converter / game (call once reviewed).
    LevelSpec toLevelSpec() const {
        LevelSpec spec;
        spec.wallHeight = wallHeight;
        spec.wallThickness = wallThickness;
        spec.walls = walls;
        for (const ReviewSymbol& s : symbols) {
            spec.symbols.push_back(Symbol{s.type, s.position, 0.0f});
        }
        return spec;
    }
};

/**
 * @brief Assemble an editable InterpretedLevel from the CV outputs.
 *
 * Wall polyline points and symbol centroids (in image pixels) are scaled by
 * @p worldScale into world units. A symbol flagged low-confidence by the
 * recognizer is marked needsReview so the user is prompted to confirm it.
 */
inline InterpretedLevel assembleLevel(const std::vector<cv::Polyline>& wallPolylines,
                                      const std::vector<cv::SymbolInstance>& symbols,
                                      float worldScale = 1.0f, float wallHeight = 3.0f,
                                      float wallThickness = 0.2f) {
    InterpretedLevel level;
    level.wallHeight = wallHeight;
    level.wallThickness = wallThickness;

    for (const cv::Polyline& poly : wallPolylines) {
        Wall w;
        for (const cv::Point& p : poly) {
            w.polyline.push_back(ecs::Vec3{p.x * worldScale, 0.0f, p.y * worldScale});
        }
        level.walls.push_back(std::move(w));
    }
    for (const cv::SymbolInstance& s : symbols) {
        ReviewSymbol r;
        r.type = s.result.object;
        r.position = ecs::Vec3{s.cx * worldScale, 0.0f, s.cy * worldScale};
        r.confidence = s.result.confidence;
        r.needsReview = s.result.lowConfidence;
        r.reviewed = false;
        level.symbols.push_back(std::move(r));
    }
    return level;
}

} // namespace game
} // namespace IKore
