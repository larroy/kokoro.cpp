#pragma once
#include "NumberNormalizer.h"

#include <optional>
#include <string>

enum class Meridiem { None, Am, Pm };

// One clock time as written: "07:30:15 PM" -> {7, 30, 15, true, Pm}.
struct ClockTime {
    int hour;                   // 0-23 as written (1-12 when meridiem != None)
    int minute;                 // 0-59
    std::optional<int> second;  // 0-59; empty when the text has no seconds
    bool padded_hour;           // hour written with a leading zero ("07")
    Meridiem meridiem;
};

// Words for `time` in `language`, which must be resolved (English, Spanish, German or Chinese, never Auto).
// "10:49 AM" -> "ten forty nine ay em", "diez y cuarenta y nueve a eme", "zehn uhr neunundvierzig", "上午十点四十九分".
std::string speak_time(const ClockTime& time, NumberLanguage language);
