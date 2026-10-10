#pragma once
#include <string>

#include "GermanLexicon.h"

// Kokoro phonemes (espeak-ng `de` convention) for one part; stressed: ˈ before its stressed vowel.
std::string part_to_kokoro(const LexiconPart& part, bool stressed);
