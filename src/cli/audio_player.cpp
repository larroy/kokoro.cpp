#include "audio_player.h"

#include <miniaudio.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace kokoro_cli {

constexpr double kTailSilence_s = 0.2;

struct AudioPlayer::Impl {
    ma_device device{};
    bool device_open = false;  // enqueue thread and destructor only
    int sample_rate = 0;

    std::mutex mutex;
    std::condition_variable idle_cv;
    std::deque<kokoro_audio> clips;
    size_t cursor = 0;
    std::vector<kokoro_audio> finished;
    size_t trailing_silence_frames = 0;
    size_t tail_frames = 0;

    ~Impl() {
        for (kokoro_audio& clip : clips) kokoro_audio_free(&clip);
        for (kokoro_audio& clip : finished) kokoro_audio_free(&clip);
    }

    // Consumes queued clips into `out`; used-up clips move to `finished` (never freed on the
    // audio thread). Remaining space is silence; the trailing-silence count signals idleness.
    void fill(float* out, size_t frames) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t written = 0;
        while (written < frames && !clips.empty()) {
            kokoro_audio& clip = clips.front();
            const size_t remaining = clip.num_samples - cursor;
            const size_t take = std::min(remaining, frames - written);
            std::copy_n(clip.samples + cursor, take, out + written);
            written += take;
            cursor += take;
            if (cursor == clip.num_samples) {
                finished.push_back(clip);
                clips.pop_front();
                cursor = 0;
            }
        }
        const size_t silent_frames = frames - written;
        std::fill(out + written, out + frames, 0.0f);
        if (clips.empty()) {
            trailing_silence_frames = (written > 0 ? 0 : trailing_silence_frames) + silent_frames;
            if (trailing_silence_frames >= tail_frames) idle_cv.notify_all();
        } else {
            trailing_silence_frames = 0;
        }
    }

    static void data_callback(ma_device* device, void* out, const void*, ma_uint32 frames) {
        static_cast<Impl*>(device->pUserData)->fill(static_cast<float*>(out), frames);
    }

    bool open(int sample_rate_hz, std::string& error) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 1;
        config.sampleRate = sample_rate_hz;
        config.dataCallback = &Impl::data_callback;
        config.pUserData = this;
        const ma_result init_result = ma_device_init(nullptr, &config, &device);
        if (init_result != MA_SUCCESS) {
            error = "cannot open audio device: " + std::string(ma_result_description(init_result));
            return false;
        }
        device_open = true;
        const ma_result start_result = ma_device_start(&device);
        if (start_result != MA_SUCCESS) {
            error = "cannot start audio device: " + std::string(ma_result_description(start_result));
            ma_device_uninit(&device);
            device_open = false;
            return false;
        }
        sample_rate = sample_rate_hz;
        tail_frames = static_cast<size_t>(static_cast<double>(sample_rate_hz) * kTailSilence_s);
        return true;
    }
};

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {}

AudioPlayer::~AudioPlayer() {
    if (impl_->device_open) ma_device_uninit(&impl_->device);  // stops the callback first
}

bool AudioPlayer::enqueue(kokoro_audio& audio, std::string& error) {
    kokoro_audio clip = audio;
    audio = {};
    if (clip.num_samples == 0 || clip.samples == nullptr) {
        kokoro_audio_free(&clip);
        return true;
    }
    if (!impl_->device_open && !impl_->open(clip.sample_rate, error)) {
        kokoro_audio_free(&clip);
        return false;
    }
    if (clip.sample_rate != impl_->sample_rate) {
        error = "sample rate changed from " + std::to_string(impl_->sample_rate) + " to " +
                std::to_string(clip.sample_rate) + " Hz";
        kokoro_audio_free(&clip);
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        for (kokoro_audio& old : impl_->finished) kokoro_audio_free(&old);
        impl_->finished.clear();
        impl_->clips.push_back(clip);
        impl_->trailing_silence_frames = 0;
    }
    return true;
}

void AudioPlayer::wait_idle() {
    if (!impl_->device_open) return;
    std::unique_lock<std::mutex> lock(impl_->mutex);
    for (;;) {
        const bool idle = impl_->clips.empty() && impl_->trailing_silence_frames >= impl_->tail_frames;
        // The state check keeps an unplugged or dead device from hanging exit.
        if (idle || ma_device_get_state(&impl_->device) != ma_device_state_started) return;
        impl_->idle_cv.wait_for(lock, std::chrono::milliseconds(100));
    }
}

void AudioPlayer::clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (kokoro_audio& clip : impl_->clips) kokoro_audio_free(&clip);
    impl_->clips.clear();
    impl_->cursor = 0;
}

}  // namespace kokoro_cli
