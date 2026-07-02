#pragma once

// The full Doodlebound drawing -> scene pipeline, as one portable header.
#include "cv/Image.h"
#include "cv/Rectify.h"
#include "cv/Symbols.h"
#include "cv/Topology.h"
#include "cv/Vectorize.h"
#include "game/DoodleScene.h"
#include "game/DungeonGame.h"
#include "game/LevelFormat.h"
#include "game/LevelReview.h"
#include "game/TourCamera.h"

#include <string>
#include <vector>

/**
 * @file Doodle.h
 * @brief Portable, renderer-agnostic Doodlebound converter library (issue #171).
 *
 * Milestone 17. A single public header that packages the drawing -> scene pipeline
 * (Milestones 15-16) as a standalone library usable from a mobile app or any
 * engine. Every piece is header-only and depends only on the C++ standard library
 * and the engine's glm-free value types - no renderer, no OpenGL, no glm - so it
 * builds independently of the desktop renderer and is covered by isolated tests.
 *
 * The facade in @c IKore::doodle wires the stages into a few calls:
 *   interpretPhoto : camera image  -> reviewable InterpretedLevel
 *   buildScene     : level         -> renderer-agnostic SceneDescription
 *   saveLevel/loadLevel            -> level JSON round-trip
 * The host reviews/repairs the InterpretedLevel (never fails silently), then draws
 * the SceneDescription (mesh + boxes + spawns) with its own renderer, or drives
 * the headless DungeonGame / TourCamera directly.
 */
namespace IKore {
namespace doodle {

// --- Public type aliases (the library's surface) ---------------------------
using Vec3 = ecs::Vec3;
using Image = cv::Image;
using Polyline = cv::Polyline;
using SymbolInstance = cv::SymbolInstance;
using Topology = cv::Topology;

using Wall = game::Wall;
using Symbol = game::Symbol;
using LevelSpec = game::LevelSpec;
using SceneDescription = game::SceneDescription;
using Mesh = game::Mesh;
using EntitySpawn = game::EntitySpawn;
using InterpretedLevel = game::InterpretedLevel;
using LevelIssue = game::LevelIssue;
using DungeonGame = game::DungeonGame;
using TourController = game::TourController;

using game::convert;
using game::loadGame;

/// Options for the whole pipeline (bundles the per-stage options).
struct Options {
    bool rectifyFirst{true};       ///< false if the image is already top-down.
    int rectifiedSize{256};        ///< output size of the rectify step.
    cv::VectorizeOptions vectorize{};
    cv::SymbolOptions symbols{};
    float worldScale{1.0f};        ///< world units per image pixel.
    float wallHeight{3.0f};
    float wallThickness{0.2f};
};

/**
 * @brief Interpret a camera image into a reviewable level: rectify (optional),
 *        vectorize walls, recognize symbols, and assemble an editable level.
 */
inline InterpretedLevel interpretPhoto(const Image& photo, const Options& opt = {}) {
    const Image top = opt.rectifyFirst ? cv::rectify(photo, opt.rectifiedSize) : photo;
    const std::vector<Polyline> walls = cv::vectorizeWalls(top, opt.vectorize);
    const std::vector<SymbolInstance> symbols = cv::detectSymbols(top, opt.symbols);
    return game::assembleLevel(walls, symbols, opt.worldScale, opt.wallHeight, opt.wallThickness);
}

/// Build the renderer-agnostic 3D scene (mesh + boxes + spawns) from a level.
inline SceneDescription buildScene(const LevelSpec& spec) { return game::convert(spec); }

/// Build the scene directly from a reviewed InterpretedLevel.
inline SceneDescription buildScene(const InterpretedLevel& level) {
    return game::convert(level.toLevelSpec());
}

/// Analyze room/doorway topology from a level's walls.
inline Topology analyzeTopology(const std::vector<Polyline>& walls, float gridSize = 10.0f) {
    return cv::buildTopology(walls, gridSize);
}

/// Serialize a level to the doodle-level JSON format.
inline std::string saveLevel(const LevelSpec& spec) { return game::toLevelJson(spec); }

/// Parse a level from the doodle-level JSON format.
inline bool loadLevel(const std::string& text, LevelSpec& out) {
    return game::fromLevelJson(text, out);
}

} // namespace doodle
} // namespace IKore
