#include "ClockTime.h"

#include "Utils.h"

namespace {

// Words appended for the meridiem: "" when the time has none.
std::string meridiem_words(Meridiem meridiem, const char* am, const char* pm) {
    if (meridiem == Meridiem::Am) return am;
    if (meridiem == Meridiem::Pm) return pm;
    return "";
}

int seconds_of(const ClockTime& time) { return time.second.value_or(0); }

bool ends_with(const std::string& words, const std::string& suffix) {
    return words.size() >= suffix.size() && words.compare(words.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ---- English ----

std::string english_number(int n) { return number_to_english(std::to_string(n)); }

// "thirty minutes", "one minute".
std::string english_unit(int n, const char* singular) {
    return english_number(n) + " " + singular + (n == 1 ? "" : "s");
}

// Times an English speaker reads off a 12-hour clock: AM/PM given, or an unpadded hour 1-12; never with seconds.
bool english_twelve_hour_form(const ClockTime& time) {
    if (time.second) return false;
    return time.meridiem != Meridiem::None || (!time.padded_hour && time.hour >= 1 && time.hour <= 12);
}

// "ten o'clock", "ten oh five", "ten forty nine"; minute 0 with AM/PM is just the hour ("ten ay em").
std::string english_twelve_hour(const ClockTime& time) {
    const std::string hour = english_number(time.hour);
    if (time.minute == 0) return time.meridiem == Meridiem::None ? hour + " o'clock" : hour;
    return hour + (time.minute < 10 ? " oh " : " ") + english_number(time.minute);
}

// "nineteen hours twenty nine minutes thirty seconds"; zero minutes and seconds are left out.
std::string english_with_units(const ClockTime& time) {
    std::string words = english_unit(time.hour, "hour");
    if (time.minute > 0) words += " " + english_unit(time.minute, "minute");
    if (seconds_of(time) > 0) words += " " + english_unit(seconds_of(time), "second");
    return words;
}

std::string english_time(const ClockTime& time) {
    const std::string clock = english_twelve_hour_form(time) ? english_twelve_hour(time) : english_with_units(time);
    return clock + meridiem_words(time.meridiem, " ay em", " pee em");
}

// ---- Spanish ----

std::string spanish_number(int n) { return number_to_spanish(std::to_string(n)); }

// Hours are feminine (la una, las veintiuna).
std::string spanish_hour(int hour) {
    if (hour == 1) return "una";
    if (hour == 21) return "veintiuna";
    return spanish_number(hour);
}

// A final "uno" before a masculine noun: "veintiún segundos", "treinta y un segundos".
std::string spanish_before_noun(std::string words) {
    if (ends_with(words, "veintiuno")) return words.substr(0, words.size() - 3) + "ún";
    if (ends_with(words, "uno")) words.pop_back();
    return words;
}

// "diez y cuarenta y nueve", "diez en punto", "diecinueve horas".
std::string spanish_hour_and_minute(const ClockTime& time) {
    const std::string hour = spanish_hour(time.hour);
    if (time.minute > 0) return hour + " y " + spanish_number(time.minute);
    if (!time.padded_hour && time.hour >= 1 && time.hour <= 12) return hour + " en punto";
    return hour + (time.hour == 1 ? " hora" : " horas");
}

std::string spanish_seconds(int seconds) {
    if (seconds == 0) return "";
    if (seconds == 1) return " con un segundo";
    return " con " + spanish_before_noun(spanish_number(seconds)) + " segundos";
}

std::string spanish_time(const ClockTime& time) {
    return spanish_hour_and_minute(time) + spanish_seconds(seconds_of(time)) +
           meridiem_words(time.meridiem, " a eme", " pe eme");
}

// ---- German ----

std::string german_number(int n) { return number_to_german(std::to_string(n)); }

// German speaks the 24-hour clock: "10:49 PM" is 22 Uhr.
int german_hour(const ClockTime& time) {
    if (time.meridiem == Meridiem::Pm) return time.hour % 12 + 12;
    if (time.meridiem == Meridiem::Am) return time.hour % 12;
    return time.hour;
}

// "zehn uhr neunundvierzig und dreißig sekunden", "ein uhr".
std::string german_time(const ClockTime& time) {
    const int hour = german_hour(time);
    std::string words = (hour == 1 ? std::string("ein") : german_number(hour)) + " uhr";
    if (time.minute > 0) words += " " + german_number(time.minute);
    const int seconds = seconds_of(time);
    if (seconds == 1) words += " und eine sekunde";
    if (seconds > 1) words += " und " + german_number(seconds) + " sekunden";
    return words;
}

// ---- Chinese ----

std::string chinese_number(int n) { return BasicStringUtil::NumberToChinese(std::to_string(n)); }

// "四十九分", "零五分"; minute 0 is silent unless seconds follow ("零分").
std::string chinese_minute(int minute, int seconds) {
    if (minute == 0) return seconds > 0 ? "零分" : "";
    return (minute < 10 ? "零" : "") + chinese_number(minute) + "分";
}

// "上午十点四十九分", "两点", "十点零分三十秒".
std::string chinese_time(const ClockTime& time) {
    const int seconds = seconds_of(time);
    const std::string hour = time.hour == 2 ? "两" : chinese_number(time.hour);
    return meridiem_words(time.meridiem, "上午", "下午") + hour + "点" + chinese_minute(time.minute, seconds) +
           (seconds > 0 ? chinese_number(seconds) + "秒" : "");
}

}  // namespace

std::string speak_time(const ClockTime& time, NumberLanguage language) {
    switch (language) {
        case NumberLanguage::Spanish: return spanish_time(time);
        case NumberLanguage::German: return german_time(time);
        case NumberLanguage::Chinese: return chinese_time(time);
        default: return english_time(time);
    }
}
