// Test for OGG Vorbis + MP3 decoding to PCM - issue #258.
//
// Decodes REAL embedded test vectors (tests/data_audio_vectors.h): a 1-second
// 440 Hz mono sine at 22050 Hz, amplitude 0.5, encoded once with libvorbis (via
// libsndfile 1.2.2) and once with LAME (via lameenc), then checks the decoded
// PCM's format, duration, dominant frequency (zero-crossing rate), and peak
// amplitude against those known ground-truth parameters. Also covers garbage
// rejection and the extension-based decodeAudio router.
//
// Two-file build (the vendored decoders live in one implementation TU):
//   g++ -std=c++17 -I src tests/test_audio_compressed.cpp src/audio/AudioDecodeCompressed.cpp -o test_audio_compressed

#include "audio/AudioDecodeCompressed.h"

#include "data_audio_vectors.h"

#include <cmath>
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

struct ToneStats {
    double freqHz{0.0};
    int peak{0};
};

// Dominant frequency via zero-crossing rate, plus the peak sample magnitude.
static ToneStats analyzeTone(const PcmAudio& a) {
    ToneStats st;
    const std::vector<std::int16_t> s = asInt16(a);
    if (s.size() < 2 || a.sampleRate <= 0) return st;
    int zc = 0;
    int peak = 0;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if ((s[i - 1] < 0) != (s[i] < 0)) ++zc;
        const int m = s[i] < 0 ? -s[i] : s[i];
        if (m > peak) peak = m;
    }
    const double seconds = static_cast<double>(a.frameCount()) / a.sampleRate;
    st.freqHz = seconds > 0.0 ? zc / 2.0 / seconds : 0.0;
    st.peak = peak;
    return st;
}

static void testDecodeOggGroundTruth() {
    PcmAudio a;
    std::string err;
    CHECK(decodeOgg(kTestToneOgg, kTestToneOgg_len, a, err));
    CHECK(a.valid());
    CHECK(a.channels == 1);
    CHECK(a.sampleRate == 22050);
    CHECK(a.bitsPerSample == 16);
    // Vorbis reproduces the exact 1-second length.
    CHECK(a.frameCount() == 22050u);

    const ToneStats st = analyzeTone(a);
    std::printf("[info] ogg decode: %zu frames, est %.1f Hz, peak %d\n", a.frameCount(), st.freqHz, st.peak);
    CHECK(st.freqHz > 425.0 && st.freqHz < 455.0);   // the encoded 440 Hz tone
    CHECK(st.peak > 13000 && st.peak < 20000);       // ~0.5 * 32767, lossy tolerance
}

static void testDecodeMp3GroundTruth() {
    PcmAudio a;
    std::string err;
    CHECK(decodeMp3(kTestToneMp3, kTestToneMp3_len, a, err));
    CHECK(a.valid());
    CHECK(a.channels == 1);
    CHECK(a.sampleRate == 22050);
    CHECK(a.bitsPerSample == 16);
    // MP3 adds encoder delay/padding frames around the 1-second signal.
    CHECK(a.frameCount() >= 22050u && a.frameCount() <= 26000u);

    const ToneStats st = analyzeTone(a);
    std::printf("[info] mp3 decode: %zu frames, est %.1f Hz, peak %d\n", a.frameCount(), st.freqHz, st.peak);
    CHECK(st.freqHz > 420.0 && st.freqHz < 465.0);   // 440 Hz, padding skews the estimate slightly
    CHECK(st.peak > 13000 && st.peak < 20000);
}

static void testMalformedRejected() {
    PcmAudio a;
    std::string err;
    const unsigned char junk[] = "this is definitely not audio data at all";
    CHECK(!decodeOgg(junk, sizeof(junk), a, err));
    CHECK(!err.empty());
    CHECK(!decodeMp3(junk, sizeof(junk), a, err));
    CHECK(!err.empty());
    CHECK(!decodeOgg(nullptr, 0, a, err));
    CHECK(!decodeMp3(nullptr, 0, a, err));
    // Feeding the wrong container to a decoder fails rather than mis-decoding.
    CHECK(!decodeOgg(kTestToneMp3, kTestToneMp3_len, a, err));
}

static void testDecodeAudioRouting() {
    PcmAudio a;
    std::string err;
    // Routes by extension, case-insensitively.
    CHECK(decodeAudio("tone.ogg", kTestToneOgg, kTestToneOgg_len, a, err));
    CHECK(a.sampleRate == 22050 && a.frameCount() == 22050u);
    CHECK(decodeAudio("TONE.MP3", kTestToneMp3, kTestToneMp3_len, a, err));
    CHECK(a.frameCount() >= 22050u);
    // Unknown extensions fall back to WAV parsing, which rejects these bytes.
    CHECK(!decodeAudio("tone.xyz", kTestToneOgg, kTestToneOgg_len, a, err));
}

int main() {
    testDecodeOggGroundTruth();
    testDecodeMp3GroundTruth();
    testMalformedRejected();
    testDecodeAudioRouting();

    if (g_failures == 0) {
        std::printf("All compressed audio decode tests passed.\n");
        return 0;
    }
    std::printf("%d compressed audio decode test(s) failed.\n", g_failures);
    return 1;
}
