// End-to-end checks against the real model.
// usage: test_synthesis <model.onnx> <voices.bin> <dict_dir> [doctest options]
// Exits 77 (CTest skip) when the model or voices file is missing.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <kokoro/kokoro.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

kokoro_ctx* g_ctx = nullptr;
const char* g_model_path = nullptr;
const char* g_voices_path = nullptr;
const char* g_dict_dir = nullptr;

const char* const kText = "你好，世界。";
const char* const kVoice = "zf_002";

std::vector<float> synth(kokoro_ctx* ctx, const char* text, const char* voice, float speed, unsigned flags,
                         kokoro_status* status) {
    kokoro_audio audio{};
    *status = kokoro_synthesize(ctx, text, voice, speed, flags, &audio);
    std::vector<float> samples(audio.samples, audio.samples + audio.num_samples);
    kokoro_audio_free(&audio);
    return samples;
}

std::vector<float> synth(const char* text, const char* voice, float speed, unsigned flags, kokoro_status* status) {
    return synth(g_ctx, text, voice, speed, flags, status);
}

// Minimal reader/writer for the voices file format (docs/adding-voices.md).
using VoiceTable = std::map<std::string, std::vector<float>>;
constexpr size_t kStyleDim = 256;
constexpr size_t kStyleRows = 510;

uint32_t read_u32(std::ifstream& in) {
    uint32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return v;
}

void write_u32(std::ofstream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

VoiceTable read_voices(const char* path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    char magic[4] = {};
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "VOIC", 4) != 0 || read_u32(in) != 1) throw std::runtime_error("bad voices file");
    VoiceTable voices;
    for (uint32_t count = read_u32(in); in && count > 0; --count) {
        std::string name(read_u32(in), '\0');
        in.read(&name[0], static_cast<std::streamsize>(name.size()));
        std::vector<float> style(read_u32(in));
        in.read(reinterpret_cast<char*>(style.data()), static_cast<std::streamsize>(style.size() * sizeof(float)));
        voices[name] = std::move(style);
    }
    if (!in) throw std::runtime_error("truncated voices file");
    return voices;
}

void write_voices(const std::filesystem::path& path, const VoiceTable& voices) {
    std::ofstream out(path, std::ios::binary);
    out.write("VOIC", 4);
    write_u32(out, 1);
    write_u32(out, static_cast<uint32_t>(voices.size()));
    for (const auto& [name, style] : voices) {
        write_u32(out, static_cast<uint32_t>(name.size()));
        out.write(name.data(), static_cast<std::streamsize>(name.size()));
        write_u32(out, static_cast<uint32_t>(style.size()));
        out.write(reinterpret_cast<const char*>(style.data()), static_cast<std::streamsize>(style.size() * sizeof(float)));
    }
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

}  // namespace

TEST_CASE("voices_are_listed_sorted") {
    const size_t n = kokoro_voice_count(g_ctx);
    REQUIRE(n > 0);

    bool has_default_voice = false;
    const char* prev = nullptr;
    for (size_t i = 0; i < n; ++i) {
        const char* cur = kokoro_voice_name(g_ctx, i);
        CAPTURE(i);
        REQUIRE(cur != nullptr);
        if (prev) CHECK(std::strcmp(prev, cur) < 0);
        if (std::strcmp(cur, kVoice) == 0) has_default_voice = true;
        prev = cur;
    }
    CHECK(kokoro_voice_name(g_ctx, n) == nullptr);
    CHECK(has_default_voice);
}

TEST_CASE("phonemize_matches_g2p") {
    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, "中国", &phonemes) == KOKORO_OK);
    CHECK(std::string(phonemes) == "ʈʂʊ→ŋkwo↗");
    kokoro_string_free(phonemes);
}

TEST_CASE("synthesize_produces_audio") {
    kokoro_audio audio{};
    REQUIRE(kokoro_synthesize(g_ctx, kText, kVoice, 1.0f, 0, &audio) == KOKORO_OK);
    CHECK(audio.sample_rate == 24000);
    REQUIRE(audio.num_samples > 0);

    bool all_finite = true;
    float peak = 0.0f;
    for (size_t i = 0; i < audio.num_samples; ++i) {
        all_finite = all_finite && std::isfinite(audio.samples[i]);
        peak = std::fmax(peak, std::fabs(audio.samples[i]));
    }
    CHECK(all_finite);
    CHECK(peak > 1e-3f);

    kokoro_audio_free(&audio);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
}

TEST_CASE("phoneme_input_matches_text_input") {
    kokoro_status text_status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> from_text = synth("你好", kVoice, 1.0f, 0, &text_status);

    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, "你好", &phonemes) == KOKORO_OK);
    kokoro_status phoneme_status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> from_phonemes = synth(phonemes, kVoice, 1.0f, KOKORO_INPUT_PHONEMES, &phoneme_status);
    kokoro_string_free(phonemes);

    CHECK(text_status == KOKORO_OK);
    CHECK(phoneme_status == KOKORO_OK);
    // The vocoder injects random noise, so samples differ run to run even for identical input;
    // the duration predictor is deterministic, so the same phonemes yield the same length.
    CHECK(!from_text.empty());
    CHECK(from_text.size() == from_phonemes.size());
}

TEST_CASE("faster_speed_is_shorter") {
    kokoro_status normal_status = KOKORO_ERROR_UNKNOWN;
    kokoro_status fast_status = KOKORO_ERROR_UNKNOWN;
    const size_t normal = synth(kText, kVoice, 1.0f, 0, &normal_status).size();
    const size_t fast = synth(kText, kVoice, 2.0f, 0, &fast_status).size();
    CHECK(normal_status == KOKORO_OK);
    CHECK(fast_status == KOKORO_OK);
    CHECK(fast < normal);
}

TEST_CASE("unknown_voice_is_reported") {
    kokoro_audio audio{};
    CHECK(kokoro_synthesize(g_ctx, kText, "no_such_voice", 1.0f, 0, &audio) == KOKORO_ERROR_VOICE_NOT_FOUND);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
    CHECK(audio.sample_rate == 0);
    CHECK(std::string(kokoro_last_error()).find("no_such_voice") != std::string::npos);
}

TEST_CASE("invalid_speed_and_flags_rejected") {
    const float bad_speeds[] = {0.0f, -1.0f, NAN, INFINITY};
    for (float speed : bad_speeds) {
        CAPTURE(speed);
        kokoro_audio audio{};
        CHECK(kokoro_synthesize(g_ctx, kText, kVoice, speed, 0, &audio) == KOKORO_ERROR_INVALID_ARGUMENT);
    }
    kokoro_audio audio{};
    CHECK(kokoro_synthesize(g_ctx, kText, kVoice, 1.0f, 0x2u, &audio) == KOKORO_ERROR_INVALID_ARGUMENT);
}

// A chunk of n phoneme tokens must use style row n - 1, as upstream Kokoro does (`pack[len(ps)-1]`).
// Each probe voice is zf_002 with row 4 taken from af_maple, so on a 5-token input it matches
// af_maple's deterministic duration only if row 4 is the row read. probe_short ends at row 4,
// which covers the last row of a table (row 509 of a full one).
TEST_CASE("style_row_follows_token_count") {
    const char* const input = "nixau";  // 5 phonemes, all in the vocabulary: one token each
    const size_t row = 4;

    VoiceTable voices = read_voices(g_voices_path);
    const std::vector<float>& donor = voices.at("af_maple");
    std::vector<float> probe = voices.at("zf_002");
    REQUIRE(probe.size() == kStyleRows * kStyleDim);
    std::copy_n(donor.begin() + row * kStyleDim, kStyleDim, probe.begin() + row * kStyleDim);
    voices["probe_short"].assign(probe.begin(), probe.begin() + (row + 1) * kStyleDim);
    voices["probe"] = std::move(probe);
    const auto probe_path = std::filesystem::temp_directory_path() / "kokoro_test_probe_voices.bin";
    write_voices(probe_path, voices);

    kokoro_ctx* ctx = nullptr;
    const kokoro_status created = kokoro_create(g_model_path, probe_path.u8string().c_str(), g_dict_dir, &ctx);
    std::filesystem::remove(probe_path);
    REQUIRE_MESSAGE(created == KOKORO_OK, kokoro_last_error());

    auto length = [&](const char* voice) {
        kokoro_status status = KOKORO_ERROR_UNKNOWN;
        const size_t n = synth(ctx, input, voice, 1.0f, KOKORO_INPUT_PHONEMES, &status).size();
        CAPTURE(voice);
        CHECK(status == KOKORO_OK);
        return n;
    };
    const size_t with_donor = length("af_maple");
    // Otherwise a wrong row could not be told apart from the right one.
    REQUIRE(with_donor != length("zf_002"));
    CHECK(length("probe") == with_donor);
    CHECK(length("probe_short") == with_donor);
    kokoro_destroy(ctx);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: test_synthesis <model.onnx> <voices.bin> <dict_dir> [doctest options]\n");
        return 2;
    }
    if (!std::filesystem::exists(std::filesystem::u8path(argv[1])) ||
        !std::filesystem::exists(std::filesystem::u8path(argv[2]))) {
        std::printf("SKIP: model or voices file not found (%s, %s)\n", argv[1], argv[2]);
        return 77;
    }
    g_model_path = argv[1];
    g_voices_path = argv[2];
    g_dict_dir = argv[3];
    if (kokoro_create(g_model_path, g_voices_path, g_dict_dir, &g_ctx) != KOKORO_OK) {
        std::fprintf(stderr, "kokoro_create failed: %s\n", kokoro_last_error());
        return 1;
    }

    std::vector<char*> dt_args{argv[0]};
    dt_args.insert(dt_args.end(), argv + 4, argv + argc);
    doctest::Context context;
    context.applyCommandLine(static_cast<int>(dt_args.size()), dt_args.data());
    const int rc = context.run();
    kokoro_destroy(g_ctx);
    return rc;
}
