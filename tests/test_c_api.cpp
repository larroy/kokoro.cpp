// C API contract checks that need no model files.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <kokoro/kokoro.h>

#include <string>
#include <thread>

extern "C" const char* kokoro_c_header_version(void);

TEST_CASE("version_matches_project") {
    CHECK(std::string(kokoro_version()) == KOKORO_EXPECTED_VERSION);
}

TEST_CASE("header_compiles_as_c") {
    CHECK(std::string(kokoro_c_header_version()) == kokoro_version());
}

TEST_CASE("create_rejects_null_arguments") {
    CHECK(kokoro_create("m", "v", "d", nullptr) == KOKORO_ERROR_INVALID_ARGUMENT);
    CHECK(kokoro_last_error()[0] != '\0');

    int sentinel = 0;
    kokoro_ctx* ctx = reinterpret_cast<kokoro_ctx*>(&sentinel);
    CHECK(kokoro_create(nullptr, "v", "d", &ctx) == KOKORO_ERROR_INVALID_ARGUMENT);
    CHECK(ctx == nullptr);
}

TEST_CASE("create_reports_load_failure") {
    int sentinel = 0;
    kokoro_ctx* ctx = reinterpret_cast<kokoro_ctx*>(&sentinel);
    CHECK(kokoro_create("does-not-exist.onnx", "does-not-exist.bin", "does-not-exist", &ctx) == KOKORO_ERROR_LOAD);
    CHECK(ctx == nullptr);
    CHECK(kokoro_last_error()[0] != '\0');
}

TEST_CASE("last_error_is_thread_local") {
    REQUIRE(kokoro_create("m", "v", "d", nullptr) == KOKORO_ERROR_INVALID_ARGUMENT);

    std::string other_thread_error = "unset";
    std::thread worker([&] { other_thread_error = kokoro_last_error(); });
    worker.join();

    CHECK(other_thread_error.empty());
    CHECK(kokoro_last_error()[0] != '\0');
}

TEST_CASE("calls_reject_null_context") {
    CHECK(kokoro_voice_count(nullptr) == 0);
    CHECK(kokoro_voice_name(nullptr, 0) == nullptr);

    int sentinel = 0;
    kokoro_audio audio{reinterpret_cast<float*>(&sentinel), 5, 1};
    CHECK(kokoro_synthesize(nullptr, "x", "zf_002", 1.0f, 0, &audio) == KOKORO_ERROR_INVALID_ARGUMENT);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
    CHECK(audio.sample_rate == 0);
    CHECK(kokoro_synthesize(nullptr, "x", "zf_002", 1.0f, 0, nullptr) == KOKORO_ERROR_INVALID_ARGUMENT);

    char* phonemes = reinterpret_cast<char*>(&sentinel);
    CHECK(kokoro_phonemize(nullptr, "x", &phonemes) == KOKORO_ERROR_INVALID_ARGUMENT);
    CHECK(phonemes == nullptr);
}

TEST_CASE("audio_free_zeroes_struct") {
    kokoro_audio audio{nullptr, 5, 24000};
    kokoro_audio_free(&audio);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
    CHECK(audio.sample_rate == 0);
}
