// End-to-end checks of the Spanish voices against the Kokoro v1.0 model.
// usage: test_synthesis_es <model.onnx> <voices.bin> <dict_dir> [doctest options]
// Exits 77 (CTest skip) when the model or voices file is missing.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <kokoro/kokoro.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

kokoro_ctx* g_ctx = nullptr;

const char* const kHolaMundo = "ˈola mˈundo";  // spanish_g2p golden in test_g2p.cpp

std::vector<float> synth(const char* text, const char* voice, unsigned flags, kokoro_status* status) {
    kokoro_audio audio{};
    *status = kokoro_synthesize(g_ctx, text, voice, 1.0f, flags, &audio);
    std::vector<float> samples(audio.samples, audio.samples + audio.num_samples);
    kokoro_audio_free(&audio);
    return samples;
}

std::string ph(const char* text) {
    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, text, &phonemes) == KOKORO_OK);
    const std::string result = phonemes;
    kokoro_string_free(phonemes);
    return result;
}

}  // namespace

TEST_CASE("spanish_voices_synthesize") {
    for (const char* voice : {"ef_dora", "em_alex", "em_santa"}) {
        CAPTURE(std::string(voice));
        kokoro_audio audio{};
        REQUIRE(kokoro_synthesize(g_ctx, "Hola, ¿cómo estás?", voice, 1.0f, 0, &audio) == KOKORO_OK);
        CHECK(audio.sample_rate == 24000);
        bool all_finite = audio.num_samples > 0;
        float peak = 0.0f;
        for (size_t i = 0; i < audio.num_samples; ++i) {
            all_finite = all_finite && std::isfinite(audio.samples[i]);
            peak = std::fmax(peak, std::fabs(audio.samples[i]));
        }
        CHECK(all_finite);
        CHECK(peak > 1e-3f);
        kokoro_audio_free(&audio);
    }
}

TEST_CASE("spanish_voice_selects_spanish_g2p") {
    const std::string p_en = ph("Hola mundo");
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_SPANISH) == KOKORO_OK);
    const std::string p_es = ph("Hola mundo");
    CHECK(p_es == kHolaMundo);
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_AUTO) == KOKORO_OK);
    REQUIRE(p_en != p_es);

    kokoro_status text_status = KOKORO_ERROR_UNKNOWN;
    kokoro_status es_status = KOKORO_ERROR_UNKNOWN;
    kokoro_status en_status = KOKORO_ERROR_UNKNOWN;
    const size_t n_text = synth("Hola mundo", "ef_dora", 0, &text_status).size();
    const size_t n_es = synth(p_es.c_str(), "ef_dora", KOKORO_INPUT_PHONEMES, &es_status).size();
    const size_t n_en = synth(p_en.c_str(), "ef_dora", KOKORO_INPUT_PHONEMES, &en_status).size();
    CHECK(text_status == KOKORO_OK);
    CHECK(es_status == KOKORO_OK);
    CHECK(en_status == KOKORO_OK);
    // The duration predictor is deterministic: the same phonemes yield the same length (see test_synthesis.cpp).
    REQUIRE(n_es != n_en);
    CHECK(n_text == n_es);
}

TEST_CASE("set_language_rejects_unknown") {
    CHECK(kokoro_set_language(g_ctx, static_cast<kokoro_language>(7)) == KOKORO_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("number_language_precedence") {
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_SPANISH) == KOKORO_OK);
    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_AUTO) == KOKORO_OK);
    CHECK(ph("3") == ph("tres"));
    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_ENGLISH) == KOKORO_OK);
    CHECK(ph("3") == ph("three"));

    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_AUTO) == KOKORO_OK);
    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_SPANISH) == KOKORO_OK);
    CHECK(ph("3") == ph("tres"));
    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_AUTO) == KOKORO_OK);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: test_synthesis_es <model.onnx> <voices.bin> <dict_dir> [doctest options]\n");
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
