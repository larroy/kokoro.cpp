// 16-bit PCM encoders for kokoro's mono float audio.
#ifndef KOKORO_SERVER_AUDIO_ENCODING_H
#define KOKORO_SERVER_AUDIO_ENCODING_H

#include <cstddef>
#include <string>

namespace kokoro_server {

// Raw signed 16-bit little-endian mono samples.
std::string encode_pcm16(const float* samples, size_t count);

// 44-byte RIFF/WAVE header followed by encode_pcm16 data. Throws std::length_error past the 4 GiB WAV limit.
std::string encode_wav_pcm16(const float* samples, size_t count, int sample_rate);

}  // namespace kokoro_server

#endif  // KOKORO_SERVER_AUDIO_ENCODING_H
