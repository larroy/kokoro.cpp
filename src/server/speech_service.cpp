#include "speech_service.h"

#include <algorithm>

namespace kokoro_server {
namespace {

std::vector<std::string> list_voices(const kokoro_ctx* ctx) {
    std::vector<std::string> voices;
    const size_t count = kokoro_voice_count(ctx);
    voices.reserve(count);
    for (size_t i = 0; i < count; ++i) voices.emplace_back(kokoro_voice_name(ctx, i));
    return voices;
}

}  // namespace

SpeechService::SpeechService(kokoro_ctx* ctx) : ctx_(ctx, &kokoro_destroy), voices_(list_voices(ctx)) {}

const std::vector<std::string>& SpeechService::voices() const {
    return voices_;
}

bool SpeechService::has_voice(const std::string& voice) const {
    return std::binary_search(voices_.begin(), voices_.end(), voice);
}

SynthesisOutcome SpeechService::synthesize(const SpeechRequest& request, kokoro_audio& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    const kokoro_status status =
        kokoro_synthesize(ctx_.get(), request.input.c_str(), request.voice.c_str(), request.speed, 0u, &out);
    // kokoro_last_error() is thread_local: read it on this thread, before any other kokoro call.
    return {status, status == KOKORO_OK ? std::string() : std::string(kokoro_last_error())};
}

}  // namespace kokoro_server
