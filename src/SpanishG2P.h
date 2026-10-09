#pragma once
#include "NumberNormalizer.h"
#include "SpanishLoanwords.h"

#include <string>

// Phonemes in Kokoro's espeak-ng `es` convention (IPA, ˈ before the stressed vowel) for UTF-8 Spanish text.
// numbers: how digits are read; Auto means Spanish. Characters other than Spanish letters and kept punctuation are
// dropped. Words listed in loanwords are read from their respelling.
std::string spanish_to_phonemes(const std::string& text, NumberLanguage numbers,
                                const SpanishLoanwords& loanwords = SpanishLoanwords::builtin());
