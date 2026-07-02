#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/**
 * @file AudioDecode.h
 * @brief Dependency-free audio decoding for the sound pipeline (issue #258).
 *
 * A portable, unit-testable core for turning encoded audio bytes into raw PCM,
 * plus a procedural tone generator. It exists so both the OpenAL engine
 * (loadWAV) and the ECS SoundComponent load real audio through one robust,
 * tested path instead of a fragile fixed-header parse / a synthesized tone.
 *
 *   - parseWav: a robust RIFF/WAVE PCM parser that walks the chunk list, so
 *     files with extra chunks (LIST, fact, ...) or an extended fmt chunk parse,
 *     not just the canonical 44-byte layout. Little-endian reads are done by
 *     byte assembly (no type-punning), and malformed input is rejected with a
 *     message. Supports mono/stereo, 8- or 16-bit PCM.
 *   - generateTone: a sine PCM generator (the tested replacement for the inline
 *     placeholder tone).
 *   - asInt16 / detectFormat: sample-format and extension helpers.
 *
 * OGG/MP3 decoding needs an external decoder (libvorbis / libsndfile or a
 * vendored single-header decoder) and is out of scope for this dependency-free
 * core; detectFormat still classifies them so callers can route accordingly.
 * Header-only, std only.
 */
namespace IKore {
namespace audio {

/// Raw interleaved little-endian PCM plus its format.
struct PcmAudio {
    int channels{0};
    int sampleRate{0};
    int bitsPerSample{0};
    std::vector<std::uint8_t> data;

    std::size_t bytesPerFrame() const {
        return static_cast<std::size_t>(channels) * static_cast<std::size_t>(bitsPerSample / 8);
    }
    std::size_t frameCount() const {
        const std::size_t bpf = bytesPerFrame();
        return bpf ? data.size() / bpf : 0;
    }
    /// True when the format is one OpenAL can accept directly (8/16-bit, 1-2 ch).
    bool valid() const {
        return channels >= 1 && channels <= 2 && sampleRate > 0 &&
               (bitsPerSample == 8 || bitsPerSample == 16) && !data.empty();
    }
};

namespace detail {
inline std::uint16_t readU16LE(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8));
}
inline std::uint32_t readU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
inline bool tag4(const std::uint8_t* p, const char* s) { return std::memcmp(p, s, 4) == 0; }
} // namespace detail

/**
 * @brief Parse RIFF/WAVE PCM bytes into @p out. Returns false with @p err set on
 *        malformed or unsupported input (non-PCM, >2 channels, bits not 8/16).
 */
inline bool parseWav(const std::uint8_t* bytes, std::size_t n, PcmAudio& out, std::string& err) {
    out = PcmAudio{};
    if (n < 12 || !detail::tag4(bytes, "RIFF") || !detail::tag4(bytes + 8, "WAVE")) {
        err = "not a RIFF/WAVE file";
        return false;
    }

    bool gotFmt = false, gotData = false;
    int channels = 0, sampleRate = 0, bits = 0;
    std::size_t pos = 12;
    while (pos + 8 <= n) {
        const std::uint8_t* id = bytes + pos;
        std::uint32_t size = detail::readU32LE(bytes + pos + 4);
        const std::size_t payload = pos + 8;
        if (detail::tag4(id, "fmt ")) {
            if (payload + 16 > n || size < 16) {
                err = "truncated fmt chunk";
                return false;
            }
            const std::uint16_t audioFormat = detail::readU16LE(bytes + payload);
            channels = detail::readU16LE(bytes + payload + 2);
            sampleRate = static_cast<int>(detail::readU32LE(bytes + payload + 4));
            bits = detail::readU16LE(bytes + payload + 14);
            // 1 = PCM, 0xFFFE = WAVE_FORMAT_EXTENSIBLE (treated as PCM here).
            if (audioFormat != 1 && audioFormat != 0xFFFE) {
                err = "unsupported (non-PCM) WAV format";
                return false;
            }
            gotFmt = true;
        } else if (detail::tag4(id, "data")) {
            // Tolerate a data size that overruns the buffer (streamed/truncated
            // files): clamp to what is actually present.
            std::size_t avail = n - payload;
            std::size_t take = size <= avail ? size : avail;
            out.data.assign(bytes + payload, bytes + payload + take);
            gotData = true;
        }
        // Chunks are word-aligned: advance past the payload plus any pad byte.
        pos = payload + size + (size & 1u);
    }

    if (!gotFmt) { err = "missing fmt chunk"; return false; }
    if (!gotData) { err = "missing data chunk"; return false; }
    if (channels < 1 || channels > 2) { err = "unsupported channel count"; return false; }
    if (bits != 8 && bits != 16) { err = "unsupported bit depth"; return false; }

    out.channels = channels;
    out.sampleRate = sampleRate;
    out.bitsPerSample = bits;
    return true;
}

inline bool parseWav(const std::vector<std::uint8_t>& bytes, PcmAudio& out, std::string& err) {
    return parseWav(bytes.data(), bytes.size(), out, err);
}

/// Generate a mono 16-bit sine tone. @p amplitude in [0, 1].
inline PcmAudio generateTone(float freqHz, float seconds, int sampleRate = 44100, float amplitude = 0.3f) {
    PcmAudio a;
    a.channels = 1;
    a.sampleRate = sampleRate > 0 ? sampleRate : 44100;
    a.bitsPerSample = 16;
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    const std::size_t frames = seconds > 0.0f
        ? static_cast<std::size_t>(seconds * static_cast<float>(a.sampleRate) + 0.5f)
        : 0;
    a.data.resize(frames * 2u);
    const float twoPi = 6.28318530717958647692f;
    for (std::size_t i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(a.sampleRate);
        float s = std::sin(twoPi * freqHz * t) * amplitude;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        const std::int16_t v = static_cast<std::int16_t>(s * 32767.0f);
        a.data[i * 2] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(v) & 0xFF);
        a.data[i * 2 + 1] = static_cast<std::uint8_t>((static_cast<std::uint16_t>(v) >> 8) & 0xFF);
    }
    return a;
}

/// Decode PCM samples to signed 16-bit (8-bit unsigned PCM is centered and scaled).
inline std::vector<std::int16_t> asInt16(const PcmAudio& a) {
    std::vector<std::int16_t> out;
    if (a.bitsPerSample == 16) {
        out.resize(a.data.size() / 2);
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<std::int16_t>(detail::readU16LE(&a.data[i * 2]));
        }
    } else if (a.bitsPerSample == 8) {
        out.resize(a.data.size());
        for (std::size_t i = 0; i < out.size(); ++i) {
            // 8-bit PCM is unsigned centered at 128; scale to signed 16-bit
            // (multiply, not a left shift, so a negative value is well defined).
            out[i] = static_cast<std::int16_t>((static_cast<int>(a.data[i]) - 128) * 256);
        }
    }
    return out;
}

/// Container formats the pipeline can distinguish by file extension.
enum class AudioFormat { Unknown, Wav, Ogg, Mp3 };

inline AudioFormat detectFormat(const std::string& filename) {
    auto ends = [&](const char* ext) {
        const std::size_t len = std::strlen(ext);
        if (filename.size() < len) return false;
        for (std::size_t i = 0; i < len; ++i) {
            const char c = filename[filename.size() - len + i];
            const char lc = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
            if (lc != ext[i]) return false;
        }
        return true;
    };
    if (ends(".wav")) return AudioFormat::Wav;
    if (ends(".ogg")) return AudioFormat::Ogg;
    if (ends(".mp3")) return AudioFormat::Mp3;
    return AudioFormat::Unknown;
}

} // namespace audio
} // namespace IKore
