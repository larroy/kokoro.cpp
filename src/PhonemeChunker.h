#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Splits a phoneme string into chunks of at most max_length UTF-8 characters (as PhonemeEncoder counts them, so at
// most max_length tokens) without dropping any phoneme. Splits at [.,!?;], keeping each mark with the text before it,
// then at spaces; a space-free word still too long is cut after its last tone mark (→ ↗ ↘ ↓) within the limit, else
// at exactly max_length characters. Segments are trimmed and joined with single spaces.
std::vector<std::string> split_phonemes(const std::string& phonemes, std::size_t max_length);
