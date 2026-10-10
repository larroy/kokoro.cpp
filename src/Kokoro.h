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
#include "G2PLanguage.h"
#include "NumberNormalizer.h"

class Phonemizer;
class PhonemeEncoder;
struct KoKoroConfig;

// Constants from config
const int MAX_PHONEME_LENGTH = 510; // Example value
const int SAMPLE_RATE = 24000;      // Example value
const int STYLE_DIM = 256;          // floats per style row; a voice is a table of these rows

enum class InferenceDevice { Auto, Cpu, Cuda };

struct InferenceConfig {
    InferenceDevice device = InferenceDevice::Auto;
    int gpu_id = 0;  // CUDA device ordinal
};

class Kokoro {
public:
    /** \brief Constructs a Kokoro instance.
     *
     *  Loads the ONNX model, voice embeddings, vocabulary, and G2P dictionaries.
     *
     *  \param model_path Path to the ONNX model file (UTF-8).
     *  \param voices_path Path to the voices embeddings file (UTF-8).
     *  \param dict_dir Path to directory containing vocab.txt and G2P dictionaries (UTF-8).
     *  \param inference Where inference runs (device and GPU ordinal).
     *
     *  \throws std::runtime_error if any required resource fails to load.
     */
    Kokoro(const std::string& model_path, const std::string& voices_path, const std::string& dict_dir,
           const InferenceConfig& inference);

    /** \brief Destroys the Kokoro instance and releases resources. */
    ~Kokoro();
    // Cpu or Cuda: where the model runs.
    InferenceDevice device() const { return device_; }

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
     *  Uses the Phonemizer implementation for `language` to transform text
     *  into the phoneme representation used by the synthesis engine.
     *
     *  \param text Input text to phonemize (UTF-8).
     *  \param language G2P that reads the text (see language_for()).
     *  \return Phoneme string suitable for create(..., is_phonemes=true).
     */
    std::string phonemize(const std::string& text, G2PLanguage language);

    // How G2P reads digits in later phonemize()/create() calls. An explicit language (English/Chinese/Spanish/German)
    // wins in every phonemizer; Auto reads Spanish text's numbers in Spanish, German text's in German, English text's
    // in English, and keeps the nearest-script rule for Chinese/English text.
    void set_number_language(NumberLanguage language);

    // Forces the G2P language for later create() calls by voice name; nullopt = auto (see language_for()).
    // Number reading follows set_number_language(): an explicit number language also applies to Spanish, German and
    // English text.
    void set_language(std::optional<G2PLanguage> forced);

    // The forced language if set; else by voice name: ef_/em_* Spanish, df_/dm_* German, af_/am_/bf_/bm_* English,
    // else ChineseEnglish. "" = no voice.
    G2PLanguage language_for(const std::string& voice_name) const;

    /** \brief Synthesizes audio from text using a named voice.
     *
     *  Text is converted to phonemes internally, by the G2P language_for(voice_name) selects.
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
     *  \param language G2P that reads text when is_phonemes is false.
     *  \param speed Playback speed multiplier (default 1.0).
     *  \param is_phonemes Set true if text is already a phoneme string (default false).
     *  \param trim Set true to remove silence at start/end (default true).
     *
     *  \return Pair of (audio_samples, sample_rate). Audio is 32-bit float PCM.
     */
    std::pair<std::vector<float>, int> create(
        const std::string& text,
        const std::vector<float>& voice_style,
        G2PLanguage language,
        float speed = 1.0f,
        bool is_phonemes = false,
        bool trim = true
    );

private:
    Ort::Env env_;
    Ort::Session session_{nullptr};
    InferenceDevice device_ = InferenceDevice::Cpu;
    Ort::AllocatorWithDefaultOptions allocator_;
    
    // Placeholder for voices data: map from name to vector
    std::map<std::string, std::vector<float>> voices_;
    
    std::unique_ptr<Phonemizer> phonemizer_;
    std::unique_ptr<PhonemeEncoder> encoder_;
    std::optional<G2PLanguage> forced_language_;  // nullopt: by voice name
    
    // Internal methods

    /** \brief Creates the ONNX Runtime session and sets device().
     *
     *  Device selection: Cpu always builds a CPU session. Auto uses CUDA when this ONNX Runtime
     *  build has the CUDAExecutionProvider, else CPU. Cuda requires CUDA support and never falls
     *  back. A CUDA session that fails to initialize (for example, no usable GPU) falls back to CPU
     *  with a warning on stderr under Auto, and rethrows under Cuda. The CUDA provider uses
     *  cudnn_conv_algo_search=HEURISTIC because every chunk length is a new input shape.
     *
     *  \param model_path Path to the ONNX model file (UTF-8).
     *  \param inference Where inference runs (device and GPU ordinal).
     *
     *  \throws std::runtime_error if the model file does not exist, or Cuda is requested and this
     *          ONNX Runtime build has no CUDA support.
     *  \throws Ort::Exception if session creation fails.
     */
    void create_session(const std::string& model_path, const InferenceConfig& inference);

    /** \brief Loads the voices file into voices_.
     *
     *  Reads the binary format written by scripts/export_voices.py and voice_tool.py: the magic
     *  "VOIC", a little-endian uint32 version (1) and voice count, then per voice a little-endian
     *  uint32 name length, the UTF-8 name, a uint32 float count, and that many little-endian
     *  float32 values. A voice's values must be a nonzero multiple of STYLE_DIM (256) and are kept
     *  as one vector of style rows (510 rows x 256 floats for the standard voices). Names are
     *  case-sensitive; if a name appears twice, the later entry wins.
     *
     *  \param voices_path Path to the voices file (UTF-8).
     *
     *  \throws std::runtime_error if the file cannot be opened, has a wrong magic or version,
     *          holds a voice whose float count is not a whole number of style rows, is truncated,
     *          or holds no voices.
     */
    void load_voices(const std::string& voices_path);
    
    std::pair<std::vector<float>, int> _create_audio(
        const std::string& phonemes,
        const std::vector<float>& voice,
        float speed
    );
};
