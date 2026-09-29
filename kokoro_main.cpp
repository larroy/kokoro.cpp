#include <kokoro/kokoro.h>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
// The console hands argv over in the ANSI code page; the library expects UTF-8.
static std::string ansi_to_utf8(const char* ansi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    if (wlen <= 0) return ansi;
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, &wstr[0], wlen);
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return ansi;
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], len, NULL, NULL);
    utf8.resize(len - 1);
    return utf8;
}
#endif

static std::string arg_utf8(const char* arg) {
#ifdef _WIN32
    return ansi_to_utf8(arg);
#else
    return arg;
#endif
}

void save_audio(const std::string& filename, const float* audio, size_t num_samples, int sample_rate) {
    // Simple WAV header writing
    std::ofstream file(filename, std::ios::binary);
    
    int channels = 1;
    int bits_per_sample = 32; // Float
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    int block_align = channels * bits_per_sample / 8;
    int data_size = static_cast<int>(num_samples * sizeof(float));
    int chunk_size = 36 + data_size;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunk_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    
    int subchunk1_size = 16;
    short audio_format = 3; // IEEE Float
    short num_channels = channels;
    int sample_rate_int = sample_rate;
    
    file.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    file.write(reinterpret_cast<const char*>(&num_channels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate_int), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    short bits = bits_per_sample;
    file.write(reinterpret_cast<const char*>(&bits), 2);
    
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);
    file.write(reinterpret_cast<const char*>(audio), data_size);
    
    std::cout << "Saved audio to " << filename << std::endl;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <voices.bin> <text> [dict_dir] [voice_name]" << std::endl;
        return 1;
    }

    const std::string model_path = arg_utf8(argv[1]);
    const std::string voices_path = arg_utf8(argv[2]);
    const std::string text = arg_utf8(argv[3]);
    const std::string dict_dir = argc > 4 ? arg_utf8(argv[4]) : "dict";
    const std::string voice_name = argc > 5 ? arg_utf8(argv[5]) : "zf_002";

    kokoro_ctx* ctx = nullptr;
    if (kokoro_create(model_path.c_str(), voices_path.c_str(), dict_dir.c_str(), &ctx) != KOKORO_OK) {
        std::cerr << "Error: " << kokoro_last_error() << std::endl;
        return 1;
    }

    kokoro_audio audio;
    const kokoro_status status = kokoro_synthesize(ctx, text.c_str(), voice_name.c_str(), 1.0f, 0, &audio);
    if (status != KOKORO_OK) {
        std::cerr << "Error: " << kokoro_last_error() << std::endl;
        kokoro_destroy(ctx);
        return 1;
    }

    save_audio("output.wav", audio.samples, audio.num_samples, audio.sample_rate);
    kokoro_audio_free(&audio);
    kokoro_destroy(ctx);
    return 0;
}
