// AudioPlayer: a continuously running playback device fed from a clip queue.
#ifndef KOKORO_CLI_AUDIO_PLAYER_H
#define KOKORO_CLI_AUDIO_PLAYER_H

#include <kokoro/kokoro.h>

#include <memory>
#include <string>

namespace kokoro_cli {

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Takes ownership of `audio` (zeroes it). Opens the default device at audio.sample_rate on
    // first call. Must be called from one thread only. Returns false (audio freed) on device
    // error / rate mismatch.
    bool enqueue(kokoro_audio& audio, std::string& error);

    // Blocks until queued audio + tail silence have been output.
    void wait_idle();

    // Drops queued audio immediately.
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kokoro_cli

#endif  // KOKORO_CLI_AUDIO_PLAYER_H
