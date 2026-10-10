#pragma once
#include <string>

#include "GermanLexicon.h"
#include "NumberNormalizer.h"

// Phonemes in Kokoro's espeak-ng `de` convention (ˈ before the stressed vowel) for UTF-8 German text.
// numbers: how digits are read; Auto means German. Letters outside German/Latin and unkept punctuation are dropped.
std::string german_to_phonemes(const std::string& text, NumberLanguage numbers, const GermanLexicon& lexicon);
