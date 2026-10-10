#include "SpanishPhonemizer.h"

#include <utility>
#include "SpanishG2P.h"

SpanishPhonemizer::SpanishPhonemizer(SpanishLoanwords loanwords) : loanwords_(std::move(loanwords)) {}

std::string SpanishPhonemizer::phonemize(const std::string& text, NumberLanguage numbers) {
    return spanish_to_phonemes(text, numbers, loanwords_);
}
