#pragma once

#include <cstddef>
#include <string>
#include <vector>

// UTF-8 characters as PhonemeEncoder tokenizes them: the lead byte alone sets the length (1-4 bytes), sequences are
// not validated, and a sequence truncated by the end of the string ends there.

// Byte offset just past the character starting at pos (pos < s.size()).
std::size_t utf8_next(const std::string& s, std::size_t pos);

// Number of characters in s.
std::size_t utf8_length(const std::string& s);

// s split into its characters.
std::vector<std::string> split_utf8(const std::string& s);
