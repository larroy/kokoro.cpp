// Parsing of OpenAI-compatible /v1/audio/speech request bodies.
#ifndef KOKORO_SERVER_SPEECH_REQUEST_H
#define KOKORO_SERVER_SPEECH_REQUEST_H

#include <string>

namespace kokoro_server {

enum class ResponseFormat { Wav, Pcm };

struct SpeechRequest {
    std::string input;
    std::string voice;
    float speed = 1.0f;
    ResponseFormat format = ResponseFormat::Wav;
};

struct RequestError {
    std::string message;
    std::string param;  // "" when the error is not about one field
};

// Parses an OpenAI /v1/audio/speech JSON body. Returns false and fills error on invalid input.
bool parse_speech_request(const std::string& body, const std::string& default_voice, SpeechRequest& out,
                          RequestError& error);

const char* content_type(ResponseFormat format);  // "audio/wav" or "audio/pcm"
const char* format_name(ResponseFormat format);   // "wav" or "pcm"

}  // namespace kokoro_server

#endif  // KOKORO_SERVER_SPEECH_REQUEST_H
