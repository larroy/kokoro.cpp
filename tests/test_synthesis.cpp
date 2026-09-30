// End-to-end checks against the real model.
// usage: test_synthesis <model.onnx> <voices.bin> <dict_dir> [doctest options]
// Exits 77 (CTest skip) when the model or voices file is missing.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <kokoro/kokoro.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

kokoro_ctx* g_ctx = nullptr;

const char* const kText = "你好，世界。";
const char* const kVoice = "zf_002";

std::vector<float> synth(const char* text, const char* voice, float speed, unsigned flags, kokoro_status* status) {
    kokoro_audio audio{};
    *status = kokoro_synthesize(g_ctx, text, voice, speed, flags, &audio);
    std::vector<float> samples(audio.samples, audio.samples + audio.num_samples);
    kokoro_audio_free(&audio);
    return samples;
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
    if (kokoro_create(argv[1], argv[2], argv[3], &g_ctx) != KOKORO_OK) {
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
