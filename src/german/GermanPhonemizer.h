#pragma once
#include <optional>
#include <string>

#include "GermanLexicon.h"
#include "PhonemizerBase.h"

// German text -> phonemes via german_to_phonemes. Loads the lexicon on first use, so other languages pay nothing.
class GermanPhonemizer : public PhonemizerBase {
public:
    GermanPhonemizer(std::string dict_path, std::string neural_model_path);
    std::string phonemize(const std::string& text, NumberLanguage numbers) override;

private:
    std::string dict_path_, neural_model_path_;
    std::optional<GermanLexicon> lexicon_;  // emplaced by the first phonemize()
};
