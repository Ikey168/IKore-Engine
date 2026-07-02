#include "audio/AudioDecodeCompressed.h"

#include <cstdlib>
#include <cstring>
#include <limits>

// Vendored public-domain decoders (single-translation-unit libraries; this file is
// their one implementation TU - see AudioDecodeCompressed.h). Warnings from the
// vendored code are silenced locally so the engine's own warning level stays strict.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#define STB_VORBIS_NO_PUSHDATA_API
#include "audio/vendor/stb_vorbis.c"

#define DR_MP3_IMPLEMENTATION
#include "audio/vendor/dr_mp3.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace IKore {
namespace audio {

namespace {

/// Pack interleaved int16 frames into a PcmAudio (16-bit little-endian bytes).
bool packS16(const short* samples, long long frames, int channels, int sampleRate,
             PcmAudio& out, std::string& err) {
    if (frames <= 0 || channels < 1 || sampleRate <= 0) {
        err = "decoder produced no audio";
        return false;
    }
    if (channels > 2) {
        err = "unsupported channel count (" + std::to_string(channels) + ")";
        return false;
    }
    out.channels = channels;
    out.sampleRate = sampleRate;
    out.bitsPerSample = 16;
    const std::size_t sampleCount = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
    out.data.resize(sampleCount * 2u);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const std::uint16_t v = static_cast<std::uint16_t>(samples[i]);
        out.data[i * 2] = static_cast<std::uint8_t>(v & 0xFF);
        out.data[i * 2 + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    }
    return true;
}

} // namespace

bool decodeOgg(const std::uint8_t* bytes, std::size_t n, PcmAudio& out, std::string& err) {
    out = PcmAudio{};
    if (bytes == nullptr || n == 0) {
        err = "empty OGG input";
        return false;
    }
    if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        err = "OGG input too large";
        return false;
    }
    int channels = 0;
    int sampleRate = 0;
    short* samples = nullptr;
    const int frames = stb_vorbis_decode_memory(bytes, static_cast<int>(n), &channels, &sampleRate, &samples);
    if (frames <= 0 || samples == nullptr) {
        err = "not a decodable OGG Vorbis stream";
        return false;
    }
    const bool ok = packS16(samples, frames, channels, sampleRate, out, err);
    std::free(samples);
    return ok;
}

bool decodeMp3(const std::uint8_t* bytes, std::size_t n, PcmAudio& out, std::string& err) {
    out = PcmAudio{};
    if (bytes == nullptr || n == 0) {
        err = "empty MP3 input";
        return false;
    }
    drmp3_config config;
    std::memset(&config, 0, sizeof(config));
    drmp3_uint64 frames = 0;
    drmp3_int16* samples =
        drmp3_open_memory_and_read_pcm_frames_s16(bytes, n, &config, &frames, nullptr);
    if (samples == nullptr || frames == 0) {
        if (samples != nullptr) drmp3_free(samples, nullptr);
        err = "not a decodable MP3 stream";
        return false;
    }
    const bool ok = packS16(samples, static_cast<long long>(frames),
                            static_cast<int>(config.channels),
                            static_cast<int>(config.sampleRate), out, err);
    drmp3_free(samples, nullptr);
    return ok;
}

bool decodeAudio(const std::string& filename, const std::uint8_t* bytes, std::size_t n,
                 PcmAudio& out, std::string& err) {
    switch (detectFormat(filename)) {
        case AudioFormat::Ogg:
            return decodeOgg(bytes, n, out, err);
        case AudioFormat::Mp3:
            return decodeMp3(bytes, n, out, err);
        case AudioFormat::Wav:
        case AudioFormat::Unknown:
        default:
            return parseWav(bytes, n, out, err) && out.valid();
    }
}

} // namespace audio
} // namespace IKore
