#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @file Image.h
 * @brief A minimal, dependency-free raster image for the drawing->map CV core.
 *
 * Milestone 16. A plain interleaved byte buffer (no OpenCV, no file I/O) so the
 * computer-vision steps - paper detection, perspective rectification, white
 * balance (issue #166) - are unit-testable headless. The device camera / image
 * decoder feeds pixels into this buffer in the live app.
 */
namespace IKore {
namespace cv {

struct Image {
    int width{0};
    int height{0};
    int channels{0};
    std::vector<std::uint8_t> data; ///< row-major, channel-interleaved.

    Image() = default;
    Image(int w, int h, int c)
        : width(w), height(h), channels(c),
          data(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * c, 0) {}

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }

    std::uint8_t at(int x, int y, int c) const {
        return data[(static_cast<std::size_t>(y) * width + x) * channels + c];
    }
    void set(int x, int y, int c, std::uint8_t v) {
        data[(static_cast<std::size_t>(y) * width + x) * channels + c] = v;
    }

    /// Rec.601 luminance at a pixel (or channel 0 for single-channel images).
    float luminance(int x, int y) const {
        if (channels >= 3) {
            return 0.299f * at(x, y, 0) + 0.587f * at(x, y, 1) + 0.114f * at(x, y, 2);
        }
        return channels >= 1 ? static_cast<float>(at(x, y, 0)) : 0.0f;
    }

    /// Bilinear sample of channel @p c at (fx, fy), clamped to the edges.
    float sample(float fx, float fy, int c) const {
        if (width <= 0 || height <= 0) return 0.0f;
        if (fx < 0.0f) fx = 0.0f;
        if (fy < 0.0f) fy = 0.0f;
        if (fx > width - 1.0f) fx = width - 1.0f;
        if (fy > height - 1.0f) fy = height - 1.0f;
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = x0 + 1 < width ? x0 + 1 : x0;
        const int y1 = y0 + 1 < height ? y0 + 1 : y0;
        const float tx = fx - x0, ty = fy - y0;
        const float a = at(x0, y0, c), b = at(x1, y0, c);
        const float cc = at(x0, y1, c), d = at(x1, y1, c);
        const float top = a + (b - a) * tx;
        const float bot = cc + (d - cc) * tx;
        return top + (bot - top) * ty;
    }
};

/// Round and clamp a float to a byte.
inline std::uint8_t clampByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<std::uint8_t>(v + 0.5f);
}

} // namespace cv
} // namespace IKore
