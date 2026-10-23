// kokoro-server request parsing and audio encoding, without a model.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "audio_encoding.h"
#include "speech_request.h"

#include <cstdint>
#include <string>

using kokoro_server::RequestError;
using kokoro_server::ResponseFormat;
using kokoro_server::SpeechRequest;

namespace {

// Expects body to be rejected and returns the error.
RequestError rejected(const std::string& body) {
    SpeechRequest request;
    RequestError error;
    CHECK_FALSE(kokoro_server::parse_speech_request(body, "af_maple", request, error));
    return error;
}

SpeechRequest accepted(const std::string& body) {
    SpeechRequest request;
    RequestError error;
    CHECK(kokoro_server::parse_speech_request(body, "af_maple", request, error));
    return request;
}

uint32_t read_u32(const std::string& bytes, size_t offset) {
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) value = (value << 8) | static_cast<unsigned char>(bytes[offset + i]);
    return value;
}

uint16_t read_u16(const std::string& bytes, size_t offset) {
    return static_cast<uint16_t>(static_cast<unsigned char>(bytes[offset]) |
                                 (static_cast<unsigned char>(bytes[offset + 1]) << 8));
}

}  // namespace

TEST_CASE("speech request defaults") {
    const SpeechRequest request = accepted(R"({"model":"tts-1","input":" hello ","instructions":"x","extra":1})");
    CHECK(request.input == " hello ");
    CHECK(request.voice == "af_maple");
    CHECK(request.speed == 1.0f);
    CHECK(request.format == ResponseFormat::Wav);
}

TEST_CASE("speech request accepted values") {
    const SpeechRequest request =
        accepted(R"({"input":"x","voice":"bf_vale","speed":2,"response_format":"pcm","stream_format":"audio"})");
    CHECK(request.voice == "bf_vale");
    CHECK(request.speed == 2.0f);
    CHECK(request.format == ResponseFormat::Pcm);
    CHECK(accepted(R"({"input":"x","voice":null,"speed":null,"response_format":null})").voice == "af_maple");
}

TEST_CASE("speech request errors name the offending field") {
    CHECK(rejected("not json").message == "Request body must be a JSON object");
    CHECK(rejected("[]").param.empty());
    CHECK(rejected("{}").param == "input");
    CHECK(rejected(R"({"input":" \t"})").message == "'input' must be a non-empty string");
    CHECK(rejected(R"({"input":3})").param == "input");
    CHECK(rejected(R"({"input":"x","voice":1})").message == "'voice' must be a string");
    CHECK(rejected(R"({"input":"x","speed":0})").message == "'speed' must be a number > 0");
    CHECK(rejected(R"({"input":"x","speed":"fast"})").param == "speed");
    const RequestError mp3 = rejected(R"({"input":"x","response_format":"mp3"})");
    CHECK(mp3.message == "Unsupported response_format 'mp3'; kokoro-server supports wav and pcm");
    CHECK(mp3.param == "response_format");
    const RequestError sse = rejected(R"({"input":"x","stream_format":"sse"})");
    CHECK(sse.message == "Only stream_format 'audio' is supported");
    CHECK(sse.param == "stream_format");
}

TEST_CASE("PCM16 encoding clamps and rounds to little-endian samples") {
    const float samples[] = {0.0f, 1.0f, -1.0f, 2.0f};
    CHECK(kokoro_server::encode_pcm16(samples, 4) == std::string("\x00\x00\xff\x7f\x01\x80\xff\x7f", 8));
}

TEST_CASE("WAV encoding writes a PCM16 mono header") {
    const float samples[] = {0.0f, 0.5f, -0.5f};
    const std::string wav = kokoro_server::encode_wav_pcm16(samples, 3, 24000);
    REQUIRE(wav.size() == 44 + 6);
    CHECK(wav.substr(0, 4) == "RIFF");
    CHECK(read_u32(wav, 4) == 36 + 6);
    CHECK(wav.substr(8, 8) == "WAVEfmt ");
    CHECK(read_u16(wav, 20) == 1);      // PCM
    CHECK(read_u16(wav, 22) == 1);      // mono
    CHECK(read_u32(wav, 24) == 24000);  // sample rate
    CHECK(read_u32(wav, 28) == 48000);  // byte rate
    CHECK(read_u16(wav, 34) == 16);     // bits per sample
    CHECK(wav.substr(36, 4) == "data");
    CHECK(read_u32(wav, 40) == 6);
    CHECK(wav.substr(44) == kokoro_server::encode_pcm16(samples, 3));
}
