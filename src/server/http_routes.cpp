#include "http_routes.h"

#include "audio_encoding.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace kokoro_server {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

// OpenAI-style error body; param "" is sent as null.
void send_error(httplib::Response& res, int status, const std::string& message, const std::string& param) {
    const json body = {{"error",
                        {{"message", message},
                         {"type", status < 500 ? "invalid_request_error" : "server_error"},
                         {"param", param.empty() ? json(nullptr) : json(param)},
                         {"code", nullptr}}}};
    res.status = status;
    res.set_content(body.dump(), "application/json");
    std::fprintf(stderr, "POST /v1/audio/speech %d: %s\n", status, message.c_str());
}

// Frees the synthesized samples on every path out of the handler.
struct AudioGuard {
    kokoro_audio audio{};
    AudioGuard() = default;
    AudioGuard(const AudioGuard&) = delete;
    AudioGuard& operator=(const AudioGuard&) = delete;
    ~AudioGuard() { kokoro_audio_free(&audio); }
};

int status_for(kokoro_status status) {
    return status == KOKORO_ERROR_VOICE_NOT_FOUND || status == KOKORO_ERROR_INVALID_ARGUMENT ? 400 : 500;
}

std::string encode(const kokoro_audio& audio, ResponseFormat format) {
    return format == ResponseFormat::Pcm ? encode_pcm16(audio.samples, audio.num_samples)
                                         : encode_wav_pcm16(audio.samples, audio.num_samples, audio.sample_rate);
}

void log_success(const SpeechRequest& request, const kokoro_audio& audio, double synth_s) {
    const double audio_s = audio.sample_rate > 0 ? static_cast<double>(audio.num_samples) / audio.sample_rate : 0.0;
    std::fprintf(stderr, "POST /v1/audio/speech 200 voice=%s format=%s (%.2f s audio, synthesis %.3f s)\n",
                 request.voice.c_str(), format_name(request.format), audio_s, synth_s);
}

void synthesize_into(SpeechService& service, const SpeechRequest& request, httplib::Response& res) {
    AudioGuard guard;
    const auto start = Clock::now();
    const SynthesisOutcome outcome = service.synthesize(request, guard.audio);
    if (outcome.status != KOKORO_OK) return send_error(res, status_for(outcome.status), outcome.error, "");
    const double synth_s = std::chrono::duration<double>(Clock::now() - start).count();
    try {
        res.set_content(encode(guard.audio, request.format), content_type(request.format));
    } catch (const std::length_error& error) {
        return send_error(res, 500, error.what(), "");
    }
    log_success(request, guard.audio, synth_s);
}

void handle_speech(SpeechService& service, const std::string& default_voice, const httplib::Request& req,
                   httplib::Response& res) {
    SpeechRequest request;
    RequestError error;
    if (!parse_speech_request(req.body, default_voice, request, error)) {
        return send_error(res, 400, error.message, error.param);
    }
    if (!service.has_voice(request.voice)) {
        return send_error(res, 400,
                          "Unknown voice '" + request.voice + "'; GET /v1/audio/voices lists the available voices",
                          "voice");
    }
    synthesize_into(service, request, res);
}

}  // namespace

void register_routes(httplib::Server& server, SpeechService& service, std::string default_voice) {
    server.set_payload_max_length(1 << 20);
    server.Post("/v1/audio/speech", [&service, voice = std::move(default_voice)](const httplib::Request& req,
                                                                                 httplib::Response& res) {
        handle_speech(service, voice, req, res);
    });
    server.Get("/v1/audio/voices", [&service](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"voices", service.voices()}}.dump(), "application/json");
    });
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });
}

}  // namespace kokoro_server
