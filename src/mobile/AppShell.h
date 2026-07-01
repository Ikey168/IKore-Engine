#pragma once

#include "doodle/Doodle.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file AppShell.h
 * @brief Platform-agnostic mobile app shell for Doodle Dungeon (issue #172).
 *
 * Milestone 17. The device build pairs an OpenGL ES renderer and native
 * Android/iOS touch events with this headless shell, which owns the actual app
 * logic: capture drawing strokes from touch, rasterize them for the converter
 * library (#171), and drive the draw -> snap -> play -> share loop.
 *
 * Everything here is renderer- and platform-independent (std + the doodle
 * library), so the loop is unit-testable off-device; the GLES/native layer only
 * feeds touch events + frame timing and draws the resulting scene and game.
 */
namespace IKore {
namespace mobile {

enum class TouchPhase { Down, Move, Up };

struct TouchEvent {
    int pointerId{0};
    TouchPhase phase{TouchPhase::Down};
    float x{0.0f};
    float y{0.0f};
};

/// A single drawn stroke: a polyline in canvas pixels, with a pen color/width.
struct Stroke {
    std::vector<cv::Point> points;
    int r{20}, g{20}, b{20}; ///< default dark = wall ink.
    int thickness{2};
};

/// Captures touch strokes and rasterizes them into an Image for interpretation.
class DrawCanvas {
public:
    int width{256};
    int height{256};
    std::vector<Stroke> strokes;

    DrawCanvas() = default;
    DrawCanvas(int w, int h) : width(w), height(h) {}

    /// Pen color/width for strokes started after this call.
    void setPen(int red, int green, int blue, int thick = 2) {
        m_penR = red;
        m_penG = green;
        m_penB = blue;
        m_penThick = thick;
    }

    /// Feed a touch event: Down starts a stroke, Move extends it, Up ends it.
    void onTouch(const TouchEvent& e) {
        switch (e.phase) {
            case TouchPhase::Down: {
                Stroke s;
                s.r = m_penR;
                s.g = m_penG;
                s.b = m_penB;
                s.thickness = m_penThick;
                s.points.push_back({e.x, e.y});
                strokes.push_back(std::move(s));
                m_active = true;
                break;
            }
            case TouchPhase::Move:
                if (m_active && !strokes.empty()) strokes.back().points.push_back({e.x, e.y});
                break;
            case TouchPhase::Up:
                if (m_active && !strokes.empty()) strokes.back().points.push_back({e.x, e.y});
                m_active = false;
                break;
        }
    }

    /// Add a complete stroke directly (e.g. a programmatic or replayed drawing).
    void addStroke(const Stroke& s) { strokes.push_back(s); }

    void clear() {
        strokes.clear();
        m_active = false;
    }

    /// Rasterize the strokes onto a white RGB image (dark walls / colored symbols).
    cv::Image rasterize() const {
        cv::Image img(width, height, 3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < 3; ++c) img.set(x, y, c, 250);
            }
        }
        for (const Stroke& s : strokes) {
            if (s.points.size() == 1) {
                stamp(img, static_cast<int>(s.points[0].x), static_cast<int>(s.points[0].y), s);
            }
            for (std::size_t i = 0; i + 1 < s.points.size(); ++i) {
                drawLine(img, static_cast<int>(s.points[i].x), static_cast<int>(s.points[i].y),
                         static_cast<int>(s.points[i + 1].x), static_cast<int>(s.points[i + 1].y), s);
            }
        }
        return img;
    }

private:
    void stamp(cv::Image& img, int cx, int cy, const Stroke& s) const {
        for (int dy = -s.thickness; dy <= s.thickness; ++dy) {
            for (int dx = -s.thickness; dx <= s.thickness; ++dx) {
                const int x = cx + dx, y = cy + dy;
                if (img.inBounds(x, y)) {
                    img.set(x, y, 0, static_cast<std::uint8_t>(s.r));
                    img.set(x, y, 1, static_cast<std::uint8_t>(s.g));
                    img.set(x, y, 2, static_cast<std::uint8_t>(s.b));
                }
            }
        }
    }
    void drawLine(cv::Image& img, int x0, int y0, int x1, int y1, const Stroke& s) const {
        const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            stamp(img, x0, y0, s);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    bool m_active{false};
    int m_penR{20}, m_penG{20}, m_penB{20}, m_penThick{2};
};

enum class AppState { Draw, Review, Play, Share };

/// The draw -> snap -> play -> share loop over the doodle converter library.
class AppShell {
public:
    AppShell() : m_canvas(256, 256) {}
    explicit AppShell(int canvasW, int canvasH) : m_canvas(canvasW, canvasH) {}

    AppState state() const { return m_state; }
    DrawCanvas& canvas() { return m_canvas; }
    doodle::InterpretedLevel& level() { return m_level; }
    const doodle::DungeonGame& game() const { return m_game; }
    const std::string& sharePayload() const { return m_share; }
    doodle::Options& options() { return m_options; }

    /// Route a touch event to the canvas while drawing.
    void onDrawTouch(const TouchEvent& e) {
        if (m_state == AppState::Draw) m_canvas.onTouch(e);
    }

    /// Draw -> Review: rasterize the drawing and interpret it into a level.
    void snap() {
        doodle::Options o = m_options;
        o.rectifyFirst = false; // the canvas is already a clean top-down drawing
        m_level = doodle::interpretPhoto(m_canvas.rasterize(), o);
        m_state = AppState::Review;
    }

    /// Review -> Play. Refuses (returns false) until the level is ready to play,
    /// so an unresolved interpretation never silently starts a broken game.
    bool startPlay() {
        if (m_state != AppState::Review || !m_level.readyToPlay()) return false;
        m_game = doodle::loadGame(doodle::buildScene(m_level));
        m_state = AppState::Play;
        return true;
    }

    /// Set the current movement input (e.g. from a virtual joystick / drag).
    void setMoveInput(float dx, float dz) {
        m_moveX = dx;
        m_moveZ = dz;
    }

    /// Advance the game one frame while playing.
    void update(float dt) {
        if (m_state == AppState::Play) m_game.update(game::GameInput{m_moveX, m_moveZ}, dt);
    }

    bool gameFinished() const { return m_game.won() || m_game.lost(); }

    /// Produce the shareable level (JSON) and enter the Share state.
    void share() {
        m_share = doodle::saveLevel(m_level.toLevelSpec());
        m_state = AppState::Share;
    }

    /// Start over with a fresh drawing.
    void newDrawing() {
        m_canvas.clear();
        m_level = doodle::InterpretedLevel{};
        m_share.clear();
        m_moveX = m_moveZ = 0.0f;
        m_state = AppState::Draw;
    }

private:
    AppState m_state{AppState::Draw};
    DrawCanvas m_canvas;
    doodle::Options m_options;
    doodle::InterpretedLevel m_level;
    doodle::DungeonGame m_game;
    std::string m_share;
    float m_moveX{0.0f};
    float m_moveZ{0.0f};
};

} // namespace mobile
} // namespace IKore
