#include "audio_encoding.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace kokoro_server {
namespace {

void put_u16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFFu));
    out.push_back(static_cast<char>((v >> 8) & 0xFFu));
}

void put_u32(std::string& out, uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>((v >> shift) & 0xFFu));
}

int16_t to_pcm16(float sample) {
    return static_cast<int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f));
}

}  // namespace

std::string encode_pcm16(const float* samples, size_t count) {
    std::string out;
    out.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) put_u16(out, static_cast<uint16_t>(to_pcm16(samples[i])));
    return out;
}

std::string encode_wav_pcm16(const float* samples, size_t count, int sample_rate) {
    if (count > (0xFFFFFFFFu - 36u) / 2u) throw std::length_error("audio too long for a WAV file");
    const uint32_t data_size = static_cast<uint32_t>(count * 2);
    const uint32_t rate = static_cast<uint32_t>(sample_rate);

    std::string out;
    out.reserve(44u + data_size);
    out += "RIFF";
    put_u32(out, 36u + data_size);
    out += "WAVE";
    out += "fmt ";
    put_u32(out, 16);        // fmt chunk size
    put_u16(out, 1);         // WAVE_FORMAT_PCM
    put_u16(out, 1);         // channels
    put_u32(out, rate);
    put_u32(out, rate * 2);  // byte rate
    put_u16(out, 2);         // block align
    put_u16(out, 16);        // bits per sample
    out += "data";
    put_u32(out, data_size);
    out += encode_pcm16(samples, count);
    return out;
}

}  // namespace kokoro_server
