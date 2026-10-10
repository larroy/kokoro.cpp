#pragma once
#include <string>
#include "PhonemizerBase.h"
#include "SpanishLoanwords.h"

// Spanish text -> phonemes via spanish_to_phonemes. Auto numbers read as Spanish.
class SpanishPhonemizer : public PhonemizerBase {
public:
    explicit SpanishPhonemizer(SpanishLoanwords loanwords);
    std::string phonemize(const std::string& text, NumberLanguage numbers) override;

private:
    SpanishLoanwords loanwords_;
};
