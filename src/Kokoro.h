#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <optional>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <onnxruntime_cxx_api.h>

// Forward declarations or placeholder for dependencies
class Tokenizer;
struct KoKoroConfig;

// Constants from config
const int MAX_PHONEME_LENGTH = 510; // Example value
const int SAMPLE_RATE = 24000;      // Example value
const int STYLE_DIM = 256;          // floats per style row; a voice is a table of these rows

class Kokoro {
public:
    // Throws std::runtime_error when the model, voices, vocab or G2P dictionaries cannot be loaded.
    // dict_dir holds vocab.txt and the G2P dictionaries. Paths are UTF-8.
    Kokoro(const std::string& model_path, const std::string& voices_path, const std::string& dict_dir);
    ~Kokoro();

    // Throws std::out_of_range when the voice does not exist.
    const std::vector<float>& get_voice_style(const std::string& name) const;
    std::vector<std::string> voice_names() const;
    // Text -> phoneme string as consumed by create(..., is_phonemes = true).
    std::string phonemize(const std::string& text);

    std::pair<std::vector<float>, int> create(
        const std::string& text,
        const std::string& voice_name,
        float speed = 1.0f,
        bool is_phonemes = false,
        bool trim = true
    );

    // Overload for passing voice style directly
    std::pair<std::vector<float>, int> create(
        const std::string& text,
        const std::vector<float>& voice_style,
        float speed = 1.0f,
        bool is_phonemes = false,
        bool trim = true
    );

private:
    Ort::Env env_;
    Ort::Session session_{nullptr};
    Ort::AllocatorWithDefaultOptions allocator_;
    
    // Placeholder for voices data: map from name to vector
    std::map<std::string, std::vector<float>> voices_;
    
    std::unique_ptr<Tokenizer> tokenizer_;
    
    // Internal methods
    void load_voices(const std::string& voices_path);
    
    std::pair<std::vector<float>, int> _create_audio(
        const std::string& phonemes,
        const std::vector<float>& voice,
        float speed
    );

    std::vector<std::string> _split_phonemes(const std::string& phonemes);
};
