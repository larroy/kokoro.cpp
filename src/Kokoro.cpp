#include "Kokoro.h"
#include "Tokenizer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <numeric>
#include <cstring>
#include <filesystem>
#include <stdexcept>

// Helper to trim audio (simple amplitude based silence removal)
std::vector<float> trim_audio(const std::vector<float>& audio, int sample_rate, float threshold_db = 60.0f) {
    // This is a simplified implementation. 
    // A proper one would compute RMS in frames.
    // For now, we just return the audio as is or do a simple amplitude trim
    return audio; 
}

// Paths cross the API as UTF-8; u8path keeps non-ASCII paths intact on Windows.
static std::filesystem::path utf8_path(const std::string& path) {
    return std::filesystem::u8path(path);
}

Kokoro::Kokoro(const std::string& model_path, const std::string& voices_path, const std::string& dict_dir)
    : env_(ORT_LOGGING_LEVEL_WARNING, "Kokoro")
{
    // Initialize session options
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Load model
    session_ = Ort::Session(env_, utf8_path(model_path).c_str(), session_options);

    // Load voices
    load_voices(voices_path);

    // Load vocab
    std::string dir = dict_dir;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += "/";
    const std::string vocab_path = dir + "vocab.txt";
    std::map<std::string, int> vocab;
    std::ifstream in(utf8_path(vocab_path));
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open vocab file: " + vocab_path);
    }
    std::string line;
    while (std::getline(in, line)) {
        // Expected format: token<TAB>id
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string token = line.substr(0, tab);
        std::string id_str = line.substr(tab + 1);
        // Unescape token if needed (\n, \r, \t)
        size_t pos = 0;
        while((pos = token.find("\\n", pos)) != std::string::npos) { token.replace(pos, 2, "\n"); pos += 1; }
        pos = 0;
        while((pos = token.find("\\r", pos)) != std::string::npos) { token.replace(pos, 2, "\r"); pos += 1; }
        pos = 0;
        while((pos = token.find("\\t", pos)) != std::string::npos) { token.replace(pos, 2, "\t"); pos += 1; }

        try {
            vocab[token] = std::stoi(id_str);
        } catch (...) {}
    }
    if (vocab.empty()) {
        throw std::runtime_error("Vocab file contains no tokens: " + vocab_path);
    }

    TokenizerConfig config;
    config.dict_dir = dir;
    tokenizer_ = std::make_unique<Tokenizer>(config, vocab);
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

std::string Kokoro::phonemize(const std::string& text) {
    return tokenizer_->phonemize(text);
}

std::vector<std::string> Kokoro::_split_phonemes(const std::string& phonemes) {
    std::vector<std::string> batches;
    std::regex re("([.,!?;])");
    std::sregex_token_iterator it(phonemes.begin(), phonemes.end(), re, {-1, 0}); // -1 for non-match, 0 for match
    std::sregex_token_iterator end;

    std::string current_batch;
    
    for (; it != end; ++it) {
        std::string part = *it;
        // Removing leading/trailing whitespace
        part = std::regex_replace(part, std::regex("^\\s+|\\s+$"), "");
        
        if (part.empty()) continue;

        if (current_batch.length() + part.length() + 1 >= MAX_PHONEME_LENGTH) {
            batches.push_back(current_batch);
            current_batch = part;
        } else {
             if (std::string(".,!?;").find(part) != std::string::npos) {
                current_batch += part;
             } else {
                if (!current_batch.empty()) current_batch += " ";
                current_batch += part;
             }
        }
    }
    if (!current_batch.empty()) {
        batches.push_back(current_batch);
    }
    return batches;
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

    std::vector<int> tokens_raw = tokenizer_->tokenize(truncated_phonemes);

    // Add start and end tokens (0)
    std::vector<int64_t> tokens = {0};
    for (int t : tokens_raw) tokens.push_back(t);
    tokens.push_back(0);
    
    // Prepare inputs
    std::vector<int64_t> input_shape = {1, (int64_t)tokens.size()};
    
    // A voice holds one style row per chunk length: n tokens use row n - 1, as upstream Kokoro
    // (`pack[len(ps)-1]`) and kokoro-onnx do. Lengths past the table use its last row; a chunk with
    // no in-vocabulary tokens uses row 0.
    const size_t rows = voice.size() / STYLE_DIM;
    const size_t row = std::min(std::max<size_t>(tokens_raw.size(), 1), rows) - 1;
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
    float speed,
    bool is_phonemes,
    bool trim
) {
    std::string phonemes = text;
    if (!is_phonemes) {
        phonemes = phonemize(text);
    }
    
    auto batched_phonemes = _split_phonemes(phonemes);
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
    return create(text, get_voice_style(voice_name), speed, is_phonemes, trim);
}
