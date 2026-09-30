#include "interactive.h"

#include "audio_player.h"
#include "console.h"
#include "speed.h"

#include <kokoro/kokoro.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace kokoro_cli {
namespace {

struct Phrase {
    std::string text;
    SynthesisSettings settings;  // copied when queued: later /voice does not change queued phrases
};

// Thread-safe queue of pending phrases.
class PhraseQueue {
public:
    void push(Phrase phrase) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            phrases_.push_back(std::move(phrase));
        }
        has_phrase_.notify_one();
    }

    // Blocks; returns nullopt once closed and empty.
    std::optional<Phrase> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        has_phrase_.wait(lock, [&] { return !phrases_.empty() || closed_; });
        if (phrases_.empty()) return std::nullopt;
        Phrase phrase = std::move(phrases_.front());
        phrases_.pop_front();
        return phrase;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        has_phrase_.notify_all();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        phrases_.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable has_phrase_;
    std::deque<Phrase> phrases_;
    bool closed_ = false;
};

// Owns ctx until run_interactive joins this thread: the C API forbids concurrent use of one
// context, and the main thread must stay free to read stdin.
void synthesis_worker(kokoro_ctx* ctx, PhraseQueue& queue, AudioPlayer& player) {
    using Clock = std::chrono::steady_clock;
    for (;;) {
        const std::optional<Phrase> phrase = queue.pop();
        if (!phrase) return;
        kokoro_audio audio{};
        const auto start = Clock::now();
        if (kokoro_synthesize(ctx, phrase->text.c_str(), phrase->settings.voice.c_str(),
                              phrase->settings.speed, phrase->settings.flags, &audio) != KOKORO_OK) {
            std::fprintf(stderr, "kokoro: %s\n", kokoro_last_error());
            continue;
        }
        const double synth_s = std::chrono::duration<double>(Clock::now() - start).count();
        const double seconds = audio.sample_rate > 0 ? static_cast<double>(audio.num_samples) / audio.sample_rate
                                                     : 0.0;
        std::fprintf(stderr, "Synthesized %.2f s audio in %.3f s\n", seconds, synth_s);
        std::string error;
        if (!player.enqueue(audio, error)) std::fprintf(stderr, "kokoro: %s\n", error.c_str());
    }
}

struct Session {
    SynthesisSettings settings;
    std::vector<std::string> voices;
};

enum class Action { Continue, Quit };

// `line` is already trimmed and starts with '/'.
Action handle_command(const std::string& line, Session& session) {
    const size_t space = line.find(' ');
    const std::string command = line.substr(0, space);
    std::string argument = space == std::string::npos ? std::string() : line.substr(space + 1);
    const size_t first = argument.find_first_not_of(" \t");
    argument = first == std::string::npos ? std::string() : argument.substr(first);
    while (!argument.empty() && (argument.back() == ' ' || argument.back() == '\t')) argument.pop_back();

    if (command == "/quit") return Action::Quit;
    if (command == "/voices") {
        for (const std::string& voice : session.voices) std::printf("%s\n", voice.c_str());
        return Action::Continue;
    }
    if (command == "/voice") {
        if (!argument.empty()) {
            if (std::find(session.voices.begin(), session.voices.end(), argument) == session.voices.end()) {
                std::fprintf(stderr, "kokoro: unknown voice '%s'; try /voices\n", argument.c_str());
                return Action::Continue;
            }
            session.settings.voice = argument;
        }
        std::fprintf(stderr, "voice: %s\n", session.settings.voice.c_str());
        return Action::Continue;
    }
    if (command == "/speed") {
        float speed = 0.0f;
        if (!argument.empty()) {
            if (!parse_speed(argument, speed)) {
                std::fprintf(stderr, "kokoro: invalid speed '%s' (must be a number > 0)\n", argument.c_str());
                return Action::Continue;
            }
            session.settings.speed = speed;
        }
        std::fprintf(stderr, "speed: %g\n", static_cast<double>(session.settings.speed));
        return Action::Continue;
    }
    if (command == "/help") {
        std::fputs("Type a phrase and press Enter to hear it. Commands:\n"
                   "  /voice [name]   show or set the voice\n"
                   "  /speed [rate]   show or set the speaking rate (> 0)\n"
                   "  /voices         list the available voices\n"
                   "  /help           show this help\n"
                   "  /quit           stop immediately, discarding queued phrases\n"
                   "End of input (Ctrl-D, or Ctrl-Z Enter on Windows) quits after queued phrases finish "
                   "playing.\n",
                   stderr);
        return Action::Continue;
    }
    std::fprintf(stderr, "kokoro: unknown command '%s'; try /help\n", command.c_str());
    return Action::Continue;
}

std::string trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

}  // namespace

int run_interactive(kokoro_ctx* ctx, const SynthesisSettings& initial) {
    Session session;
    session.settings = initial;
    const size_t count = kokoro_voice_count(ctx);
    session.voices.reserve(count);
    for (size_t i = 0; i < count; ++i) session.voices.emplace_back(kokoro_voice_name(ctx, i));

    AudioPlayer player;
    PhraseQueue queue;
    std::thread worker(synthesis_worker, ctx, std::ref(queue), std::ref(player));
    std::fprintf(stderr, "Interactive mode: type a phrase and press Enter to hear it; /help lists commands.\n");

    for (;;) {
        std::fputs("> ", stderr);
        std::fflush(stderr);
        std::string line;
        if (!read_line_utf8(line)) {
            std::fputc('\n', stderr);
            queue.close();
            worker.join();
            player.wait_idle();
            return 0;
        }
        const std::string text = trim(line);
        if (text.empty()) continue;
        if (text[0] == '/') {
            if (handle_command(text, session) == Action::Quit) {
                queue.clear();
                queue.close();
                worker.join();  // waits for the synthesis in progress, if any
                player.clear();
                return 0;
            }
            continue;
        }
        queue.push({text, session.settings});
    }
}

}  // namespace kokoro_cli
