#include "GermanPhonemizer.h"

#include <utility>

#include "GermanG2P.h"

GermanPhonemizer::GermanPhonemizer(std::string dict_path, std::string neural_model_path)
    : dict_path_(std::move(dict_path)), neural_model_path_(std::move(neural_model_path)) {}

// A context is single-threaded by contract (docs/thread-safety.md), so the lazy load needs no lock.
std::string GermanPhonemizer::phonemize(const std::string& text, NumberLanguage numbers) {
    if (!lexicon_) lexicon_.emplace(dict_path_, neural_model_path_);
    return german_to_phonemes(text, numbers, *lexicon_);
}
