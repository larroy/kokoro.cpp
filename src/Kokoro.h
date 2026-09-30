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
#include "NumberNormalizer.h"

// Forward declarations or placeholder for dependencies
class Tokenizer;
struct KoKoroConfig;

// Constants from config
const int MAX_PHONEME_LENGTH = 510; // Example value
const int SAMPLE_RATE = 24000;      // Example value
const int STYLE_DIM = 256;          // floats per style row; a voice is a table of these rows

class Kokoro {
public:
    /** \brief Constructs a Kokoro instance.
     *
     *  Loads the ONNX model, voice embeddings, vocabulary, and G2P dictionaries.
     *
     *  \param model_path Path to the ONNX model file (UTF-8).
     *  \param voices_path Path to the voices embeddings file (UTF-8).
     *  \param dict_dir Path to directory containing vocab.txt and G2P dictionaries (UTF-8).
     *
     *  \throws std::runtime_error if any required resource fails to load.
     */
    Kokoro(const std::string& model_path, const std::string& voices_path, const std::string& dict_dir);
    
    /** \brief Destroys the Kokoro instance and releases resources. */
    ~Kokoro();

    /** \brief Retrieves a voice embedding by name.
     *
     *  \param name Voice identifier (e.g., "af_bella", "af_sarah").
     *
     *  \return const reference to the voice embedding vector (256 floats).
     *  \throws std::out_of_range if the voice name is not found.
     */
    const std::vector<float>& get_voice_style(const std::string& name) const;
    
    /** \brief Returns the list of available voice identifiers.
     *
     *  \return Vector of voice names loaded from the voices file.
     */
    std::vector<std::string> voice_names() const;
    
    /** \brief Converts plain text to a phoneme string.
     *
     *  Uses the loaded G2P dictionaries and tokenizer to transform text
     *  into the phoneme representation used by the synthesis engine.
     *
     *  \param text Input text to phonemize (UTF-8).
     *  \return Phoneme string suitable for create(..., is_phonemes=true).
     */
    std::string phonemize(const std::string& text);

    // How G2P reads digits in later phonemize()/create() calls.
    void set_number_language(NumberLanguage language);

    /** \brief Synthesizes audio from text using a named voice.
     *
     *  Text is automatically converted to phonemes internally.
     *
     *  \param text Input text to synthesize (UTF-8).
     *  \param voice_name Voice identifier from voice_names().
     *  \param speed Playback speed multiplier (default 1.0).
     *  \param is_phonemes Set true if text is already a phoneme string (default false).
     *  \param trim Set true to remove silence at start/end (default true).
     *
     *  \return Pair of (audio_samples, sample_rate). Audio is 32-bit float PCM.
     */
    std::pair<std::vector<float>, int> create(
        const std::string& text,
        const std::string& voice_name,
        float speed = 1.0f,
        bool is_phonemes = false,
        bool trim = true
    );

    /** \brief Synthesizes audio from text using a raw voice embedding.
     *
     *  Useful when voice embeddings are stored externally or computed.
     *
     *  \param text Input text to synthesize (UTF-8).
     *  \param voice_style Reference to a 256-element float vector.
     *  \param speed Playback speed multiplier (default 1.0).
     *  \param is_phonemes Set true if text is already a phoneme string (default false).
     *  \param trim Set true to remove silence at start/end (default true).
     *
     *  \return Pair of (audio_samples, sample_rate). Audio is 32-bit float PCM.
     */
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
