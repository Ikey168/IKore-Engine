#pragma once

#include "audio/AudioDecode.h" // PcmAudio

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @file AudioDecodeCompressed.h
 * @brief OGG Vorbis and MP3 decoding to raw PCM (issue #258).
 *
 * Completes the compressed half of the audio decode pipeline: turn OGG/MP3 bytes
 * into the same PcmAudio that parseWav produces, so every format funnels into one
 * representation before reaching OpenAL.
 *
 * Decoding is done by vendored public-domain single-header decoders (no external
 * library dependency, matching how the repo already vendors stb for images):
 *   - src/audio/vendor/stb_vorbis.c (Sean Barrett et al., v1.22) for OGG. This is
 *     the same stb revision the build already pins for stb_image.
 *   - src/audio/vendor/dr_mp3.h (David Reid, v0.7.4) for MP3.
 *
 * The implementations live in AudioDecodeCompressed.cpp (the vendored decoders are
 * single-translation-unit libraries), so link that file alongside any target that
 * calls these. Both return 16-bit interleaved little-endian PCM; malformed input is
 * rejected with an error message rather than partial output.
 */
namespace IKore {
namespace audio {

/// Decode an OGG Vorbis stream in memory to 16-bit PCM. False + @p err on failure.
bool decodeOgg(const std::uint8_t* bytes, std::size_t n, PcmAudio& out, std::string& err);

/// Decode an MP3 stream in memory to 16-bit PCM. False + @p err on failure.
bool decodeMp3(const std::uint8_t* bytes, std::size_t n, PcmAudio& out, std::string& err);

/**
 * @brief Decode any supported container (WAV, OGG, MP3) by extension, falling back
 *        to WAV parsing for unknown extensions. One entry point for file loading.
 */
bool decodeAudio(const std::string& filename, const std::uint8_t* bytes, std::size_t n,
                 PcmAudio& out, std::string& err);

} // namespace audio
} // namespace IKore
