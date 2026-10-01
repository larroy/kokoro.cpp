#pragma once
#include <string>

// Spanish is only used when forced: normalize_numbers never resolves Auto to Spanish.
enum class NumberLanguage { Auto, English, Chinese, Spanish };

// Replaces every number ([-+]?\d+(?:\.\d+)*) in UTF-8 `text` with words in `language`.
// Auto: language of the nearest ASCII letter or CJK character before the number; if none, of the
// first such character in the text; if none, Chinese.
std::string normalize_numbers(const std::string& text, NumberLanguage language);

// English reading of one number match: "-3.14" -> "minus three point one four".
std::string number_to_english(const std::string& num_str);

// Spanish reading of one number match: "-3.14" -> "menos tres punto uno cuatro".
std::string number_to_spanish(const std::string& num_str);
