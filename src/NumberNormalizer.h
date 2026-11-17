#pragma once
#include <string>

// Spanish and German are only used when forced: normalize_numbers never resolves Auto to them.
enum class NumberLanguage { Auto, English, Chinese, Spanish, German };

// Replaces every number ([-+]?\d+(?:\.\d+)*) in UTF-8 `text` with words in `language`.
// A percent sign after the number is read too: "35 %" -> "thirty five percent", "百分之三十五".
// Auto: language of the nearest ASCII letter or CJK character before the number; if none, of the
// first such character in the text; if none, Chinese.
// Clock times H:MM[:SS] (hour 0-23, optional AM/PM, German "Uhr") are read as times: "10:49 AM" ->
// "ten forty nine ay em", "19:00" -> "nineteen hours", "10:49" -> "十点四十九分".
std::string normalize_numbers(const std::string& text, NumberLanguage language);

// English reading of one number match: "-3.14" -> "minus three point one four".
std::string number_to_english(const std::string& num_str);

// Spanish reading of one number match: "-3.14" -> "menos tres punto uno cuatro".
std::string number_to_spanish(const std::string& num_str);

// German reading of one number match: "-3.14" -> "minus drei komma eins vier".
std::string number_to_german(const std::string& num_str);
