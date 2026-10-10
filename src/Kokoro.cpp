#include "Kokoro.h"
#include "Phonemizer.h"
#include "PhonemeEncoder.h"
#include "PhonemeChunker.h"
#include <fstream>
#include <sstream>
#include <numeric>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <cstdio>
#include <algorithm>

// Helper to trim audio (simple amplitude based silence removal)
std::vector<float> trim_audio(const std::vector<float>& audio, int sample_rate, float threshold_db = 60.0f) {
    // This is a simplified implementation. 
    // A proper one would compute RMS in frames.
    // For now, we just return the audio as is or do a simple amplitude trim
    return audio; 
}

// Paths cross the API as UTF-8; u8path keeps non-ASCII paths intact on Windows.
namespace {

bool cuda_provider_available() {
    for (const auto& provider : Ort::GetAvailableProviders()) {
        if (provider == "CUDAExecutionProvider") return true;
    }
    return false;
}

Ort::SessionOptions cpu_session_options() {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    return options;
}

Ort::SessionOptions cuda_session_options(int gpu_id) {
    Ort::SessionOptions options = cpu_session_options();
    Ort::CUDAProviderOptions cuda;
    cuda.Update({{"device_id", std::to_string(gpu_id)}, {"cudnn_conv_algo_search", "HEURISTIC"}});
    options.AppendExecutionProvider_CUDA_V2(*cuda);
    return options;
}

}  // namespace

// HEURISTIC: every chunk length is a new input shape; the default EXHAUSTIVE search would re-benchmark
// each convolution per shape.
static std::filesystem::path utf8_path(const std::string& path) {
    return std::filesystem::u8path(path);
}

void Kokoro::create_session(const std::string& model_path, const InferenceConfig& inference) {
    const auto path = utf8_path(model_path);
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Model file not found: " + model_path);
    }
    const bool has_cuda = cuda_provider_available();
    const auto build_cpu_session = [&]() {
        session_ = Ort::Session(env_, path.c_str(), cpu_session_options());
        device_ = InferenceDevice::Cpu;
    };
    if (inference.device == InferenceDevice::Cpu || (inference.device == InferenceDevice::Auto && !has_cuda)) {
        build_cpu_session();
        return;
    }
    if (!has_cuda) {
        throw std::runtime_error(
            "this ONNX Runtime build has no CUDA support; install it with `uv run bootstrap.py configure --ort gpu` "
            "or `uv run bootstrap.py build-ort`");
    }
    try {
        session_ = Ort::Session(env_, path.c_str(), cuda_session_options(inference.gpu_id));
        device_ = InferenceDevice::Cuda;
    } catch (const Ort::Exception& e) {
        if (inference.device == InferenceDevice::Cuda) throw;
        std::fprintf(stderr, "kokoro: CUDA unavailable, using CPU: %s\n", e.what());
        build_cpu_session();
    }
}

Kokoro::Kokoro(const std::string& model_path, const std::string& voices_path, const std::string& dict_dir,
               const InferenceConfig& inference)
    : env_(ORT_LOGGING_LEVEL_WARNING, "Kokoro")
{
    create_session(model_path, inference);

    // Load voices
    load_voices(voices_path);

    std::string dir = dict_dir;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += "/";
    encoder_ = std::make_unique<PhonemeEncoder>(PhonemeEncoder::load(dir + "vocab.txt"));

    PhonemizerConfig config;
    config.dict_dir = dir;
    phonemizer_ = std::make_unique<Phonemizer>(config);
}

Kokoro::~Kokoro() {
    // Resources cleaned up by wrappers
}

void Kokoro::load_voices(const std::string& voices_path) {
    std::ifstream in(utf8_path(voices_path), std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open voices file: " + voices_path);
    }

    char magic[4];
    if (!in.read(magic, 4) || std::strncmp(magic, "VOIC", 4) != 0) {
        throw std::runtime_error("Invalid voices file (expected 'VOIC' header; convert voices.npy with "
                                 "scripts/export_voices.py): " + voices_path);
    }

    uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), 4);
    if (version != 1) {
        throw std::runtime_error("Unsupported voices file version " + std::to_string(version) + ": " + voices_path);
    }

    uint32_t num_voices = 0;
    in.read(reinterpret_cast<char*>(&num_voices), 4);

    for (uint32_t i = 0; in && i < num_voices; ++i) {
        uint32_t name_len = 0;
        if (!in.read(reinterpret_cast<char*>(&name_len), 4)) break;

        std::string name(name_len, '\0');
        uint32_t dim = 0;
        if (!in.read(&name[0], name_len) || !in.read(reinterpret_cast<char*>(&dim), 4)) break;
        if (dim == 0 || dim % STYLE_DIM != 0) {
            throw std::runtime_error("Invalid voice '" + name + "' in " + voices_path + ": " + std::to_string(dim) +
                                     " floats is not a whole number of " + std::to_string(STYLE_DIM) +
                                     "-float style rows");
        }

        std::vector<float> style(dim);
        if (!in.read(reinterpret_cast<char*>(style.data()), dim * sizeof(float))) break;

        voices_[name] = std::move(style);
    }

    if (!in) {
        throw std::runtime_error("Truncated voices file: " + voices_path);
    }
    if (voices_.empty()) {
        throw std::runtime_error("Voices file contains no voices: " + voices_path);
    }
}

const std::vector<float>& Kokoro::get_voice_style(const std::string& name) const {
    auto it = voices_.find(name);
    if (it == voices_.end()) {
        throw std::out_of_range("Voice not found: " + name);
    }
    return it->second;
}

std::vector<std::string> Kokoro::voice_names() const {
    std::vector<std::string> names;
    names.reserve(voices_.size());
    for (const auto& entry : voices_) names.push_back(entry.first);
    return names;
}

std::string Kokoro::phonemize(const std::string& text, G2PLanguage language) {
    return phonemizer_->phonemize(text, language);
}

void Kokoro::set_number_language(NumberLanguage language) {
    if (phonemizer_) phonemizer_->set_number_language(language);
}

void Kokoro::set_language(std::optional<G2PLanguage> forced) { forced_language_ = forced; }

G2PLanguage Kokoro::language_for(const std::string& voice_name) const {
    if (forced_language_) return *forced_language_;
    const bool kokoro_name = voice_name.size() > 3 && (voice_name[1] == 'f' || voice_name[1] == 'm') &&
                             voice_name[2] == '_';
    if (!kokoro_name) return G2PLanguage::ChineseEnglish;
    switch (voice_name[0]) {
        case 'e': return G2PLanguage::Spanish;
        case 'a': case 'b': return G2PLanguage::English;
        default: return G2PLanguage::ChineseEnglish;
    }
}

std::pair<std::vector<float>, int> Kokoro::_create_audio(
    const std::string& phonemes,
    const std::vector<float>& voice,
    float speed
) {
    std::string truncated_phonemes = phonemes;
    if (phonemes.length() > MAX_PHONEME_LENGTH) {
        truncated_phonemes = phonemes.substr(0, MAX_PHONEME_LENGTH);
    }

    std::vector<int> tokens_raw = encoder_->encode(truncated_phonemes);
    // Nothing to say: skip the model rather than run it on the padding tokens alone, as upstream
    // Kokoro skips empty phoneme strings (`if not ps: continue`).
    if (tokens_raw.empty()) return {{}, SAMPLE_RATE};

    // Add start and end tokens (0)
    std::vector<int64_t> tokens = {0};
    for (int t : tokens_raw) tokens.push_back(t);
    tokens.push_back(0);
    
    // Prepare inputs
    std::vector<int64_t> input_shape = {1, (int64_t)tokens.size()};
    
    // A voice holds one style row per chunk length: n tokens use row n - 1, as upstream Kokoro
    // (`pack[len(ps)-1]`) and kokoro-onnx do. Lengths past the table use its last row.
    const size_t rows = voice.size() / STYLE_DIM;
    const size_t row = std::min(tokens_raw.size(), rows) - 1;
    const auto style_begin = voice.begin() + row * STYLE_DIM;
    std::vector<float> selected_style(style_begin, style_begin + STYLE_DIM);

    std::vector<int64_t> style_shape = {1, (int64_t)selected_style.size()};
    std::vector<int64_t> speed_shape = {1};
    std::vector<float> speed_tensor = {speed};

   
    const char* input_names[] = {"tokens", "style", "speed"};
    const char* input_names_new[] = {"input_ids", "style", "speed"};
    
    // Querying model inputs is possible but let's just assume one set for this translation or use a check.
    // For brevity, I'll use the older "tokens" set as default or try to match python logic if I can access names.
    
    bool use_new_schema = false;
    size_t num_inputs = session_.GetInputCount();
    for(size_t i=0; i<num_inputs; i++) {
        auto name_ptr = session_.GetInputNameAllocated(i, allocator_);
        if (std::string(name_ptr.get()) == "input_ids") {
            use_new_schema = true;
        }
    }

    std::vector<const char*> inputs;
    if (use_new_schema) {
        inputs = {"input_ids", "style", "speed"};
    } else {
        inputs = {"tokens", "style", "speed"};
    }
    
    std::vector<Ort::Value> input_tensors;
    
    // Create tensors
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, tokens.data(), tokens.size(), input_shape.data(), input_shape.size()));
        
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info, selected_style.data(), selected_style.size(), style_shape.data(), style_shape.size()));
    
    int speed_int = static_cast<int>(speed);
    if (use_new_schema) {
        input_tensors.push_back(Ort::Value::CreateTensor<int>(
            memory_info, &speed_int, 1, speed_shape.data(), speed_shape.size()));
    } else {
        input_tensors.push_back(Ort::Value::CreateTensor<float>(
            memory_info, speed_tensor.data(), speed_tensor.size(), speed_shape.data(), speed_shape.size()));
    }

    // Check model output name usually
    // Or get it from session
    auto out_name_ptr = session_.GetOutputNameAllocated(0, allocator_);
    std::vector<const char*> output_names_vec = {out_name_ptr.get()};

    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        inputs.data(),
        input_tensors.data(),
        input_tensors.size(),
        output_names_vec.data(),
        1
    );
    
    // allocator_.Free(out_name, allocator_.Info()); // Handled by smart pointer

    float* floatarr = output_tensors[0].GetTensorMutableData<float>();
    size_t output_len = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
    
    std::vector<float> audio(floatarr, floatarr + output_len);
    return {audio, SAMPLE_RATE};
}

std::pair<std::vector<float>, int> Kokoro::create(
    const std::string& text,
    const std::vector<float>& voice_style,
    G2PLanguage language,
    float speed,
    bool is_phonemes,
    bool trim
) {
    std::string phonemes = text;
    if (!is_phonemes) {
        phonemes = phonemize(text, language);
    }
    
    auto batched_phonemes = split_phonemes(phonemes, MAX_PHONEME_LENGTH);
    std::vector<float> full_audio;
    
    for (const auto& batch : batched_phonemes) {
        auto [audio_part, sr] = _create_audio(batch, voice_style, speed);
        if (trim) {
            audio_part = trim_audio(audio_part, sr);
        }
        full_audio.insert(full_audio.end(), audio_part.begin(), audio_part.end());
    }
    
    return {full_audio, SAMPLE_RATE};
}

std::pair<std::vector<float>, int> Kokoro::create(
    const std::string& text,
    const std::string& voice_name,
    float speed,
    bool is_phonemes,
    bool trim
) {
    return create(text, get_voice_style(voice_name), language_for(voice_name), speed, is_phonemes, trim);
}
