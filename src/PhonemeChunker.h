#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Splits a phoneme string at [.,!?;] and packs the trimmed segments into chunks, starting a new chunk
// when the current one would reach max_length bytes. A single segment longer than that stays one chunk.
std::vector<std::string> split_phonemes(const std::string& phonemes, std::size_t max_length);
