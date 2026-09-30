// kokoro: command-line front end for libkokoro (public C API only).
#include <kokoro/kokoro.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

struct Options {
    std::string model = "models/kokoro-v1.1-zh.onnx";
    std::string voices = "models/voices-v1.1-zh.bin";
    std::string dict = "dict";
    std::string voice = "zf_002";
    float speed = 1.0f;
    std::string output = "output.wav";
    bool input_phonemes = false;  // -p/--phonemes
    bool phonemize = false;       // --phonemize
    bool list_voices = false;     // --list-voices
    bool help = false;
    bool version = false;
    bool has_text = false;
    std::string text;
};

enum class Opt { Model, Voices, Dict, Voice, Speed, Output, Phonemes, Phonemize, ListVoices, Help, Version };

struct OptionSpec {
    const char* short_name;  // nullptr if the option has no short form
    const char* long_name;
    Opt id;
    bool takes_value;
};

constexpr OptionSpec kOptions[] = {
    {"-m", "--model", Opt::Model, true},
    {nullptr, "--voices", Opt::Voices, true},
    {"-d", "--dict", Opt::Dict, true},
    {"-v", "--voice", Opt::Voice, true},
    {"-s", "--speed", Opt::Speed, true},
    {"-o", "--output", Opt::Output, true},
    {"-p", "--phonemes", Opt::Phonemes, false},
    {nullptr, "--phonemize", Opt::Phonemize, false},
    {nullptr, "--list-voices", Opt::ListVoices, false},
    {"-h", "--help", Opt::Help, false},
    {nullptr, "--version", Opt::Version, false},
};

const OptionSpec* find_option(const std::string& name) {
    for (const OptionSpec& spec : kOptions) {
        if (name == spec.long_name || (spec.short_name && name == spec.short_name)) return &spec;
    }
    return nullptr;
}

void print_usage(std::FILE* out) {
    std::fputs(
        "Usage: kokoro [options] <text>\n"
        "       kokoro --list-voices [options]\n"
        "\n"
        "Synthesize UTF-8 <text> to a WAV file with the Kokoro TTS model.\n"
        "\n"
        "Options:\n"
        "  -m, --model <path>    ONNX model file (default: models/kokoro-v1.1-zh.onnx)\n"
        "      --voices <path>   voices file (default: models/voices-v1.1-zh.bin)\n"
        "  -d, --dict <dir>      dictionary directory (default: dict)\n"
        "  -v, --voice <name>    voice to use (default: zf_002)\n"
        "  -s, --speed <rate>    speaking rate, > 0 (default: 1.0)\n"
        "  -o, --output <path>   output WAV file (default: output.wav)\n"
        "  -p, --phonemes        <text> is a phoneme string; skip G2P\n"
        "      --phonemize       print the phonemes for <text> instead of synthesizing\n"
        "      --list-voices     print the available voices and exit\n"
        "  -h, --help            print this help and exit\n"
        "      --version         print the library version and exit\n",
        out);
}

bool usage_error(const std::string& message) {
    std::fprintf(stderr, "kokoro: %s\nTry 'kokoro --help'.\n", message.c_str());
    return false;
}

#ifdef _WIN32
std::string wide_to_utf8(const wchar_t* wide) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string utf8(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &utf8[0], len, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(len) - 1);  // drop the terminator WideCharToMultiByte wrote
    return utf8;
}
#endif

// The library takes UTF-8. On Windows, argv is in the ANSI code page and loses characters outside it,
// so re-read the command line as UTF-16.
std::vector<std::string> utf8_args(int argc, char** argv) {
#ifdef _WIN32
    int count = 0;
    if (LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count)) {
        std::vector<std::string> args;
        args.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) args.push_back(wide_to_utf8(wide[i]));
        LocalFree(wide);
        return args;
    }
#endif
    return std::vector<std::string>(argv, argv + argc);
}

bool parse_speed(const std::string& value, float& speed) {
    if (value.empty()) return false;
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !std::isfinite(parsed) || !(parsed > 0.0f)) return false;
    speed = parsed;
    return true;
}

// Returns false after printing a usage error.
bool parse_args(const std::vector<std::string>& args, Options& opt) {
    bool positional_only = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (!positional_only && arg == "--") {
            positional_only = true;
            continue;
        }
        if (positional_only || arg.size() < 2 || arg[0] != '-') {
            if (opt.has_text) return usage_error("unexpected argument '" + arg + "'");
            opt.text = arg;
            opt.has_text = true;
            continue;
        }

        // Only long options accept an inline "=value".
        std::string name = arg;
        std::string value;
        bool has_value = false;
        if (arg.compare(0, 2, "--") == 0) {
            const size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                name = arg.substr(0, eq);
                value = arg.substr(eq + 1);
                has_value = true;
            }
        }

        const OptionSpec* spec = find_option(name);
        if (!spec) return usage_error("unknown option '" + arg + "'");

        if (!spec->takes_value) {
            if (has_value) return usage_error("option '" + name + "' does not take a value");
        } else if (!has_value) {
            if (i + 1 >= args.size()) return usage_error("option '" + name + "' requires a value");
            value = args[++i];
        }

        switch (spec->id) {
            case Opt::Model: opt.model = value; break;
            case Opt::Voices: opt.voices = value; break;
            case Opt::Dict: opt.dict = value; break;
            case Opt::Voice: opt.voice = value; break;
            case Opt::Output: opt.output = value; break;
            case Opt::Speed:
                if (!parse_speed(value, opt.speed)) {
                    return usage_error("invalid speed '" + value + "' (must be a number > 0)");
                }
                break;
            case Opt::Phonemes: opt.input_phonemes = true; break;
            case Opt::Phonemize: opt.phonemize = true; break;
            case Opt::ListVoices: opt.list_voices = true; break;
            case Opt::Help: opt.help = true; return true;
            case Opt::Version: opt.version = true; return true;
        }
    }

    if (opt.list_voices && opt.phonemize) return usage_error("--list-voices cannot be combined with --phonemize");
    if (opt.phonemize && opt.input_phonemes) return usage_error("--phonemize cannot be combined with --phonemes");
    if (opt.list_voices && opt.has_text) return usage_error("--list-voices does not take <text>");
    if (!opt.list_voices && !opt.has_text) return usage_error("missing <text>");
    return true;
}

void put_u16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFFu));
    out.push_back(static_cast<char>((v >> 8) & 0xFFu));
}

void put_u32(std::string& out, uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>((v >> shift) & 0xFFu));
}

// Mono 32-bit IEEE float WAV with a 44-byte header.
bool write_wav(const std::string& path, const kokoro_audio& audio, std::string& error) {
    if (audio.num_samples > (0xFFFFFFFFu - 36u) / sizeof(float)) {
        error = "audio too long for a WAV file";
        return false;
    }
    const uint32_t data_size = static_cast<uint32_t>(audio.num_samples * sizeof(float));
    const uint32_t sample_rate = static_cast<uint32_t>(audio.sample_rate);

    std::string header;
    header.reserve(44);
    header += "RIFF";
    put_u32(header, 36u + data_size);
    header += "WAVE";
    header += "fmt ";
    put_u32(header, 16);               // fmt chunk size
    put_u16(header, 3);                // WAVE_FORMAT_IEEE_FLOAT
    put_u16(header, 1);                // channels
    put_u32(header, sample_rate);
    put_u32(header, sample_rate * 4);  // byte rate
    put_u16(header, 4);                // block align
    put_u16(header, 32);               // bits per sample
    header += "data";
    put_u32(header, data_size);

    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) {
        error = "cannot open '" + path + "' for writing";
        return false;
    }
    file.write(header.data(), static_cast<std::streamsize>(header.size()));
    if (data_size > 0) file.write(reinterpret_cast<const char*>(audio.samples), static_cast<std::streamsize>(data_size));
    file.close();
    if (!file.good()) {
        error = "cannot write '" + path + "'";
        return false;
    }
    return true;
}

int library_error() {
    std::fprintf(stderr, "kokoro: %s\n", kokoro_last_error());
    return 1;
}

int run(const Options& opt) {
    kokoro_ctx* raw_ctx = nullptr;
    if (kokoro_create(opt.model.c_str(), opt.voices.c_str(), opt.dict.c_str(), &raw_ctx) != KOKORO_OK) {
        return library_error();
    }
    const std::unique_ptr<kokoro_ctx, decltype(&kokoro_destroy)> ctx(raw_ctx, &kokoro_destroy);

    if (opt.list_voices) {
        const size_t count = kokoro_voice_count(ctx.get());
        for (size_t i = 0; i < count; ++i) std::printf("%s\n", kokoro_voice_name(ctx.get(), i));
        return 0;
    }

    if (opt.phonemize) {
        char* phonemes = nullptr;
        if (kokoro_phonemize(ctx.get(), opt.text.c_str(), &phonemes) != KOKORO_OK) return library_error();
        std::printf("%s\n", phonemes);
        kokoro_string_free(phonemes);
        return 0;
    }

    kokoro_audio audio{};
    const unsigned flags = opt.input_phonemes ? KOKORO_INPUT_PHONEMES : 0u;
    if (kokoro_synthesize(ctx.get(), opt.text.c_str(), opt.voice.c_str(), opt.speed, flags, &audio) != KOKORO_OK) {
        return library_error();
    }
    std::string error;
    const bool written = write_wav(opt.output, audio, error);
    const double seconds = audio.sample_rate > 0 ? static_cast<double>(audio.num_samples) / audio.sample_rate : 0.0;
    const int sample_rate = audio.sample_rate;
    kokoro_audio_free(&audio);
    if (!written) {
        std::fprintf(stderr, "kokoro: %s\n", error.c_str());
        return 1;
    }
    std::fprintf(stderr, "Wrote %s (%.2f s, %d Hz)\n", opt.output.c_str(), seconds, sample_rate);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    Options opt;
    if (!parse_args(utf8_args(argc, argv), opt)) return 2;
    if (opt.help) {
        print_usage(stdout);
        return 0;
    }
    if (opt.version) {
        std::printf("%s\n", kokoro_version());
        return 0;
    }
    return run(opt);
}
