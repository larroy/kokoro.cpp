#include "Kokoro.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

void save_audio(const std::string& filename, const std::vector<float>& audio, int sample_rate) {
    // Simple WAV header writing
    std::ofstream file(filename, std::ios::binary);
    
    int channels = 1;
    int bits_per_sample = 32; // Float
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    int block_align = channels * bits_per_sample / 8;
    int data_size = static_cast<int>(audio.size() * sizeof(float));
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
    file.write(reinterpret_cast<const char*>(audio.data()), data_size);
    
    std::cout << "Saved audio to " << filename << std::endl;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // 打印命令行参数
    std::cout << "Command line arguments:" << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "  argv[" << i << "]: '" << argv[i] << "'" << std::endl;
    }
#endif

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <voices.bin> <text> [vocab_path] [voice_name]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string voices_path = argv[2];
    std::string text = argv[3];
    
#ifdef _WIN32
    // 将ANSI编码的文本转换为UTF-8
    std::string utf8_text;
    
    // 首先将ANSI转换为Unicode (UTF-16)
    int wstr_len = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, NULL, 0);
    if (wstr_len > 0) {
        std::wstring wstr(wstr_len, 0);
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wstr[0], wstr_len);
        
        // 然后将Unicode (UTF-16)转换为UTF-8
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        if (utf8_len > 0) {
            utf8_text.resize(utf8_len);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_text[0], utf8_len, NULL, NULL);
            utf8_text.resize(utf8_len - 1);  // 移除结尾的\0
    
            std::cout << "Converted text hex: " << utf8_text << std::endl;
       
            // 使用转换后的UTF-8文本
            text = utf8_text;
        } else {
            std::cerr << "Failed to convert from UTF-16 to UTF-8" << std::endl;
        }
    } else {
        std::cerr << "Failed to convert from ANSI to UTF-16" << std::endl;
    }
#endif
    
    std::string vocab_path = "dict/vocab.txt";
    if (argc > 4) {
        vocab_path = argv[4];
    }
    std::string voice_name = argc > 5 ? argv[5] : "zf_002";

    try {
        Kokoro tts(model_path, voices_path, vocab_path);
        
        auto voice = tts.get_voice_style(voice_name);
        auto result = tts.create(text, voice, 1.0f);
        
        save_audio("output.wav", result.first, result.second);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
