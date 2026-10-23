// kokoro-server: OpenAI-compatible text-to-speech HTTP server on top of libkokoro (public C API only).
#include <kokoro/kokoro.h>

#include "console.h"
#include "engine_options.h"
#include "http_routes.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string model = "models/kokoro-v1.1-zh.onnx";
    std::string voices = "models/voices-v1.1-zh.bin";
    std::string dict = "dict";
    std::string voice = "af_maple";
    kokoro_number_language number_language = KOKORO_NUMBERS_AUTO;  // --lang
    kokoro_language language = KOKORO_LANGUAGE_AUTO;  // --language
    kokoro_device device = KOKORO_DEVICE_AUTO;
    int gpu_id = 0;
    std::string host = "127.0.0.1";
    int port = 8880;
    bool help = false;
    bool version = false;
};

enum class Opt { Model, Voices, Dict, Voice, Lang, Language, Device, GpuId, Host, Port, Help, Version };

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
    {nullptr, "--lang", Opt::Lang, true},
    {nullptr, "--language", Opt::Language, true},
    {nullptr, "--device", Opt::Device, true},
    {nullptr, "--gpu-id", Opt::GpuId, true},
    {nullptr, "--host", Opt::Host, true},
    {nullptr, "--port", Opt::Port, true},
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
        "Usage: kokoro-server [options]\n"
        "\n"
        "Serve OpenAI-compatible text-to-speech over HTTP: POST /v1/audio/speech (wav, pcm), GET /v1/audio/voices, "
        "GET /health.\n"
        "\n"
        "Options:\n"
        "  -m, --model <path>    ONNX model file (default: models/kokoro-v1.1-zh.onnx)\n"
        "      --voices <path>   voices file (default: models/voices-v1.1-zh.bin)\n"
        "  -d, --dict <dir>      dictionary directory (default: dict)\n"
        "  -v, --voice <name>    voice used when a request omits \"voice\" (default: af_maple)\n"
        "      --lang <auto|en|zh|es|de>  language for reading numbers (default: auto)\n"
        "      --language <auto|zh|en|es|de>  text language; auto: es for ef_*/em_*, de for df_*/dm_*, en for "
        "af_/am_/bf_/bm_* voices, else zh (Chinese/English) (default: auto)\n"
        "      --device <name>   auto, cpu or cuda (default: auto)\n"
        "      --gpu-id <n>      CUDA device to use (default: 0)\n"
        "      --host <addr>     address to listen on (default: 127.0.0.1)\n"
        "      --port <n>        TCP port to listen on (default: 8880)\n"
        "  -h, --help            print this help and exit\n"
        "      --version         print the library version and exit\n",
        out);
}

bool usage_error(const std::string& message) {
    std::fprintf(stderr, "kokoro-server: %s\nTry 'kokoro-server --help'.\n", message.c_str());
    return false;
}

bool parse_port(const std::string& value, int& port) {
    if (value.empty()) return false;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end != value.c_str() + value.size() || parsed < 1 || parsed > 65535) return false;
    port = static_cast<int>(parsed);
    return true;
}

// Applies one option value; returns false after printing a usage error.
bool apply_option(Opt id, const std::string& value, Options& opt) {
    switch (id) {
        case Opt::Model: opt.model = value; return true;
        case Opt::Voices: opt.voices = value; return true;
        case Opt::Dict: opt.dict = value; return true;
        case Opt::Voice: opt.voice = value; return true;
        case Opt::Host: opt.host = value; return true;
        case Opt::Lang:
            return kokoro_cli::parse_number_language(value, opt.number_language) ||
                   usage_error("invalid number language '" + value + "' (must be auto, en, zh, es or de)");
        case Opt::Language:
            return kokoro_cli::parse_language(value, opt.language) ||
                   usage_error("invalid language '" + value + "' (must be auto, zh, en, es or de)");
        case Opt::Device:
            return kokoro_cli::parse_device(value, opt.device) ||
                   usage_error("invalid device '" + value + "' (expected auto, cpu or cuda)");
        case Opt::GpuId:
            return kokoro_cli::parse_gpu_id(value, opt.gpu_id) ||
                   usage_error("invalid GPU id '" + value + "' (must be an integer >= 0)");
        case Opt::Port:
            return parse_port(value, opt.port) || usage_error("invalid port '" + value + "' (must be 1-65535)");
        case Opt::Help: opt.help = true; return true;
        case Opt::Version: opt.version = true; return true;
    }
    return true;
}

// Returns false after printing a usage error.
bool parse_args(const std::vector<std::string>& args, Options& opt) {
    for (size_t i = 1; i < args.size() && !opt.help && !opt.version; ++i) {
        const std::string& arg = args[i];
        if (arg.size() < 2 || arg[0] != '-') return usage_error("unexpected argument '" + arg + "'");

        // Only long options accept an inline "=value".
        const size_t eq = arg.compare(0, 2, "--") == 0 ? arg.find('=') : std::string::npos;
        const std::string name = arg.substr(0, eq);
        const OptionSpec* spec = find_option(name);
        if (!spec) return usage_error("unknown option '" + arg + "'");

        std::string value;
        if (eq != std::string::npos) {
            if (!spec->takes_value) return usage_error("option '" + name + "' does not take a value");
            value = arg.substr(eq + 1);
        } else if (spec->takes_value) {
            if (i + 1 >= args.size()) return usage_error("option '" + name + "' requires a value");
            value = args[++i];
        }
        if (!apply_option(spec->id, value, opt)) return false;
    }
    return true;
}

const char* device_name(kokoro_device device) {
    switch (device) {
        case KOKORO_DEVICE_CUDA: return "cuda";
        case KOKORO_DEVICE_CPU: return "cpu";
        default: return "auto";
    }
}

int library_error() {
    std::fprintf(stderr, "kokoro-server: %s\n", kokoro_last_error());
    return 1;
}

// Creates the configured context, or returns nullptr after printing the library error.
kokoro_ctx* create_context(const Options& opt) {
    kokoro_options options = kokoro_default_options();
    options.device = opt.device;
    options.gpu_id = opt.gpu_id;
    kokoro_ctx* ctx = nullptr;
    if (kokoro_create_ex(opt.model.c_str(), opt.voices.c_str(), opt.dict.c_str(), &options, &ctx) != KOKORO_OK) {
        library_error();
        return nullptr;
    }
    if (kokoro_set_number_language(ctx, opt.number_language) != KOKORO_OK ||
        kokoro_set_language(ctx, opt.language) != KOKORO_OK) {
        library_error();
        kokoro_destroy(ctx);
        return nullptr;
    }
    return ctx;
}

// httplib's default SO_REUSEPORT lets a second server bind the same port and silently share requests.
void exclusive_listen_socket(socket_t sock) {
#ifdef _WIN32
    httplib::set_socket_opt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
    httplib::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);  // restart during TIME_WAIT only
#endif
}

int serve(kokoro_server::SpeechService& service, const Options& opt) {
    httplib::Server server;
    server.set_socket_options(exclusive_listen_socket);
    kokoro_server::register_routes(server, service, opt.voice);
    if (!server.bind_to_port(opt.host, opt.port)) {
        std::fprintf(stderr, "kokoro-server: cannot listen on %s:%d\n", opt.host.c_str(), opt.port);
        return 1;
    }
    std::fprintf(stderr, "Listening on http://%s:%d\n", opt.host.c_str(), opt.port);
    return server.listen_after_bind() ? 0 : 1;
}

int run(const Options& opt) {
    const auto init_start = std::chrono::steady_clock::now();
    kokoro_ctx* ctx = create_context(opt);
    if (!ctx) return 1;
    kokoro_server::SpeechService service(ctx);
    if (!service.has_voice(opt.voice)) {
        std::fprintf(stderr, "kokoro-server: unknown voice '%s'\n", opt.voice.c_str());
        return 1;
    }
    const double init_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - init_start).count();
    std::fprintf(stderr, "Initialized engine in %.3f s (%s)\n", init_s, device_name(kokoro_context_device(ctx)));
    return serve(service, opt);
}

}  // namespace

int main(int argc, char** argv) {
    kokoro_cli::init_console();
    Options opt;
    if (!parse_args(kokoro_cli::utf8_args(argc, argv), opt)) return 2;
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
