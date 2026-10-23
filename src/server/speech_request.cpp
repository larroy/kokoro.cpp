#include "speech_request.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace kokoro_server {
namespace {

using nlohmann::json;

bool fail(RequestError& error, std::string message, std::string param) {
    error = {std::move(message), std::move(param)};
    return false;
}

// The member, or nullptr when it is missing or JSON null.
const json* optional_member(const json& body, const char* key) {
    const auto it = body.find(key);
    return it == body.end() || it->is_null() ? nullptr : &*it;
}

bool is_blank(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

bool parse_input(const json& body, SpeechRequest& out, RequestError& error) {
    const json* input = optional_member(body, "input");
    if (!input || !input->is_string() || is_blank(input->get_ref<const std::string&>())) {
        return fail(error, "'input' must be a non-empty string", "input");
    }
    out.input = input->get<std::string>();
    return true;
}

bool parse_voice(const json& body, const std::string& default_voice, SpeechRequest& out, RequestError& error) {
    const json* voice = optional_member(body, "voice");
    if (!voice) {
        out.voice = default_voice;
        return true;
    }
    if (!voice->is_string()) return fail(error, "'voice' must be a string", "voice");
    out.voice = voice->get<std::string>();
    return true;
}

bool parse_speed(const json& body, SpeechRequest& out, RequestError& error) {
    const json* speed = optional_member(body, "speed");
    if (!speed) {
        out.speed = 1.0f;
        return true;
    }
    const double value = speed->is_number() ? speed->get<double>() : 0.0;
    if (!std::isfinite(value) || !(value > 0.0)) return fail(error, "'speed' must be a number > 0", "speed");
    out.speed = static_cast<float>(value);
    return true;
}

bool parse_format(const json& body, SpeechRequest& out, RequestError& error) {
    const json* format = optional_member(body, "response_format");
    const std::string value = !format ? "wav" : format->is_string() ? format->get<std::string>() : format->dump();
    if (value != "wav" && value != "pcm") {
        return fail(error, "Unsupported response_format '" + value + "'; kokoro-server supports wav and pcm",
                    "response_format");
    }
    out.format = value == "pcm" ? ResponseFormat::Pcm : ResponseFormat::Wav;
    return true;
}

bool check_stream_format(const json& body, RequestError& error) {
    const json* stream = optional_member(body, "stream_format");
    if (!stream || (stream->is_string() && stream->get_ref<const std::string&>() == "audio")) return true;
    return fail(error, "Only stream_format 'audio' is supported", "stream_format");
}

}  // namespace

bool parse_speech_request(const std::string& body, const std::string& default_voice, SpeechRequest& out,
                          RequestError& error) {
    const json parsed = json::parse(body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return fail(error, "Request body must be a JSON object", "");
    // model, instructions and unknown keys are accepted and ignored, as OpenAI clients send them.
    return parse_input(parsed, out, error) && parse_voice(parsed, default_voice, out, error) &&
           parse_speed(parsed, out, error) && parse_format(parsed, out, error) && check_stream_format(parsed, error);
}

const char* content_type(ResponseFormat format) {
    return format == ResponseFormat::Pcm ? "audio/pcm" : "audio/wav";
}

const char* format_name(ResponseFormat format) {
    return format == ResponseFormat::Pcm ? "pcm" : "wav";
}

}  // namespace kokoro_server
