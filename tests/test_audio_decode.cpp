// Test for the dependency-free audio decoding core - issue #258.
//
// Verifies the robust RIFF/WAVE PCM parser (canonical and non-canonical layouts,
// mono/stereo, 8/16-bit, malformed rejection, truncated tolerance), the tone
// generator, sample conversion, and format detection. These back the engine's
// loadWAV and SoundComponent's file loading, so covering them here validates real
// audio loading headlessly.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_audio_decode.cpp -o test_audio_decode

#include "audio/AudioDecode.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace IKore::audio;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static void putU16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
static void putU32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}
static void putTag(std::vector<std::uint8_t>& b, const char* t) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(t[i]));
}

// Build a WAV. If extraChunkBeforeData is non-empty, insert a chunk with that id
// (odd/even sizes both tested) between fmt and data to exercise chunk walking.
static std::vector<std::uint8_t> makeWav(int channels, int sampleRate, int bits,
                                        const std::vector<std::uint8_t>& pcm,
                                        const char* extraId = nullptr,
                                        const std::vector<std::uint8_t>& extra = {},
                                        std::uint16_t audioFormat = 1) {
    std::vector<std::uint8_t> body; // everything after "RIFF"<size>
    putTag(body, "WAVE");
    putTag(body, "fmt ");
    putU32(body, 16);
    putU16(body, audioFormat);
    putU16(body, static_cast<std::uint16_t>(channels));
    putU32(body, static_cast<std::uint32_t>(sampleRate));
    putU32(body, static_cast<std::uint32_t>(sampleRate * channels * bits / 8)); // byteRate
    putU16(body, static_cast<std::uint16_t>(channels * bits / 8));              // blockAlign
    putU16(body, static_cast<std::uint16_t>(bits));
    if (extraId) {
        putTag(body, extraId);
        putU32(body, static_cast<std::uint32_t>(extra.size()));
        for (std::uint8_t v : extra) body.push_back(v);
        if (extra.size() & 1u) body.push_back(0); // pad to even
    }
    putTag(body, "data");
    putU32(body, static_cast<std::uint32_t>(pcm.size()));
    for (std::uint8_t v : pcm) body.push_back(v);

    std::vector<std::uint8_t> out;
    putTag(out, "RIFF");
    putU32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

static void testCanonicalMono16() {
    std::vector<std::uint8_t> pcm;
    for (int i = 0; i < 8; ++i) putU16(pcm, static_cast<std::uint16_t>(i * 1000)); // 8 frames
    const std::vector<std::uint8_t> wav = makeWav(1, 8000, 16, pcm);

    PcmAudio a;
    std::string err;
    CHECK(parseWav(wav, a, err));
    CHECK(a.channels == 1 && a.sampleRate == 8000 && a.bitsPerSample == 16);
    CHECK(a.valid());
    CHECK(a.frameCount() == 8);
    CHECK(a.data.size() == pcm.size());
}

static void testStereo16AndEightBit() {
    std::vector<std::uint8_t> stereo;
    for (int i = 0; i < 4; ++i) { putU16(stereo, 100); putU16(stereo, 200); } // 4 stereo frames
    PcmAudio s;
    std::string err;
    CHECK(parseWav(makeWav(2, 44100, 16, stereo), s, err));
    CHECK(s.channels == 2 && s.frameCount() == 4); // 16 bytes / (2ch*2bytes)

    std::vector<std::uint8_t> eight = {0, 64, 128, 192, 255};
    PcmAudio e;
    CHECK(parseWav(makeWav(1, 22050, 8, eight), e, err));
    CHECK(e.bitsPerSample == 8 && e.valid() && e.frameCount() == 5);
    // 8-bit unsigned centers at 128 -> ~0 in signed 16-bit.
    const std::vector<std::int16_t> s16 = asInt16(e);
    CHECK(s16.size() == 5);
    CHECK(s16[2] == 0);      // 128 -> 0
    CHECK(s16[0] < 0);       // 0   -> negative
    CHECK(s16[4] > 0);       // 255 -> positive
}

static void testNonCanonicalChunks() {
    std::vector<std::uint8_t> pcm;
    for (int i = 0; i < 6; ++i) putU16(pcm, static_cast<std::uint16_t>(i));
    // A LIST chunk with an ODD size (7) sits before data; the parser must skip it
    // (and its pad byte) and still find data - the old fixed-44-byte parser could not.
    const std::vector<std::uint8_t> list = {'I', 'N', 'F', 'O', 'a', 'b', 'c'};
    PcmAudio a;
    std::string err;
    CHECK(parseWav(makeWav(1, 16000, 16, pcm, "LIST", list), a, err));
    CHECK(a.channels == 1 && a.sampleRate == 16000);
    CHECK(a.frameCount() == 6);
    CHECK(a.data == pcm);
}

static void testMalformedAndTruncated() {
    PcmAudio a;
    std::string err;
    // Not a RIFF file.
    std::vector<std::uint8_t> junk = {'N', 'O', 'P', 'E', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    CHECK(!parseWav(junk, a, err));
    CHECK(!err.empty());
    // Non-PCM (IEEE float, format 3) is rejected.
    std::vector<std::uint8_t> pcm(8, 0);
    CHECK(!parseWav(makeWav(1, 8000, 16, pcm, nullptr, {}, 3), a, err));
    // Missing data chunk: build a WAV then truncate before data by hand.
    std::vector<std::uint8_t> noData;
    putTag(noData, "RIFF");
    putU32(noData, 4 + 8 + 16);
    putTag(noData, "WAVE");
    putTag(noData, "fmt ");
    putU32(noData, 16);
    putU16(noData, 1); putU16(noData, 1); putU32(noData, 8000);
    putU32(noData, 16000); putU16(noData, 2); putU16(noData, 16);
    CHECK(!parseWav(noData, a, err));

    // A data chunk claiming more bytes than present is clamped, not a crash.
    std::vector<std::uint8_t> shortPcm;
    for (int i = 0; i < 4; ++i) putU16(shortPcm, 7);
    std::vector<std::uint8_t> wav = makeWav(1, 8000, 16, shortPcm);
    // Corrupt the data size to a huge value (last chunk's size field precedes the pcm).
    // data size lives at the 4 bytes just before the pcm payload at the end.
    const std::size_t sizeOff = wav.size() - shortPcm.size() - 4;
    wav[sizeOff] = 0xFF; wav[sizeOff + 1] = 0xFF; wav[sizeOff + 2] = 0xFF; wav[sizeOff + 3] = 0x7F;
    PcmAudio clamped;
    CHECK(parseWav(wav, clamped, err));
    CHECK(clamped.data.size() == shortPcm.size()); // clamped to what is present
}

static void testGenerateTone() {
    const PcmAudio t = generateTone(440.0f, 0.25f, 8000, 0.5f);
    CHECK(t.channels == 1 && t.bitsPerSample == 16 && t.sampleRate == 8000);
    CHECK(t.frameCount() == 2000); // 0.25s * 8000
    const std::vector<std::int16_t> s = asInt16(t);
    CHECK(!s.empty());
    CHECK(s[0] == 0); // sin(0) == 0
    std::int16_t peak = 0;
    for (std::int16_t v : s) { const std::int16_t m = v < 0 ? static_cast<std::int16_t>(-v) : v; if (m > peak) peak = m; }
    CHECK(peak > 8000);   // it actually oscillates
    CHECK(peak <= 16384); // within amplitude 0.5 * 32767 (plus rounding)
}

static void testDetectFormat() {
    CHECK(detectFormat("song.wav") == AudioFormat::Wav);
    CHECK(detectFormat("SONG.WAV") == AudioFormat::Wav); // case-insensitive
    CHECK(detectFormat("music.ogg") == AudioFormat::Ogg);
    CHECK(detectFormat("voice.mp3") == AudioFormat::Mp3);
    CHECK(detectFormat("clip.flac") == AudioFormat::Unknown);
    CHECK(detectFormat("noext") == AudioFormat::Unknown);
}

int main() {
    testCanonicalMono16();
    testStereo16AndEightBit();
    testNonCanonicalChunks();
    testMalformedAndTruncated();
    testGenerateTone();
    testDetectFormat();

    if (g_failures == 0) {
        std::printf("All audio decode tests passed.\n");
        return 0;
    }
    std::printf("%d audio decode test(s) failed.\n", g_failures);
    return 1;
}
