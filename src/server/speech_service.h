// SpeechService: the one kokoro context shared by all HTTP worker threads.
#ifndef KOKORO_SERVER_SPEECH_SERVICE_H
#define KOKORO_SERVER_SPEECH_SERVICE_H

#include "speech_request.h"

#include <kokoro/kokoro.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace kokoro_server {

struct SynthesisOutcome {
    kokoro_status status;
    std::string error;  // kokoro_last_error() text when status is not KOKORO_OK
};

class SpeechService {
public:
    explicit SpeechService(kokoro_ctx* ctx);  // takes ownership; destroys with kokoro_destroy

    const std::vector<std::string>& voices() const;  // sorted, as returned by kokoro_voice_name
    bool has_voice(const std::string& voice) const;

    // Serialized: the C API forbids using one context from two threads at once.
    SynthesisOutcome synthesize(const SpeechRequest& request, kokoro_audio& out);

private:
    std::unique_ptr<kokoro_ctx, decltype(&kokoro_destroy)> ctx_;
    std::vector<std::string> voices_;
    std::mutex mutex_;
};

}  // namespace kokoro_server

#endif  // KOKORO_SERVER_SPEECH_SERVICE_H
