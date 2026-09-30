#include "NumberNormalizer.h"

#include "Utils.h"

#include <regex>

namespace {

const char* const g_ones_to_nineteen[] = {"",        "one",     "two",       "three",    "four",
                                          "five",    "six",     "seven",     "eight",    "nine",
                                          "ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                                          "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
const char* const g_tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* const g_digit_words[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
const char* const g_scales[] = {"", "thousand", "million", "billion", "trillion"};

// English words for 0 < n < 100: "twenty five".
std::string below_hundred(int n) {
    if (n < 20) return g_ones_to_nineteen[n];
    const std::string tens = g_tens[n / 10];
    const int ones = n % 10;
    return ones == 0 ? tens : tens + " " + g_ones_to_nineteen[ones];
}

// English words for 0 < n < 1000: "one hundred five".
std::string below_thousand(int n) {
    const int hundreds = n / 100;
    const int rest = n % 100;
    if (hundreds == 0) return below_hundred(rest);
    const std::string result = std::string(g_ones_to_nineteen[hundreds]) + " hundred";
    return rest == 0 ? result : result + " " + below_hundred(rest);
}

// Reads every character as an isolated digit word: "168" -> "one six eight".
std::string spell_digits(const std::string& digits) {
    std::string result;
    for (const char c : digits) {
        if (!result.empty()) result += ' ';
        result += g_digit_words[c - '0'];
    }
    return result;
}

// English words for an integer digit string: "2024" -> "two thousand twenty four".
std::string integer_to_english(const std::string& digits) {
    const size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) return "zero";
    const std::string trimmed = digits.substr(first);
    const size_t len = trimmed.size();
    if (len > 15) return spell_digits(trimmed);

    std::string result;
    size_t pos = 0;
    while (pos < len) {
        const size_t rem = len - pos;
        const size_t glen = rem % 3 == 0 ? 3 : rem % 3;
        const int value = std::stoi(trimmed.substr(pos, glen));
        const int scale = static_cast<int>((len - pos - glen) / 3);
        if (value != 0) {
            if (!result.empty()) result += ' ';
            result += below_thousand(value);
            if (scale != 0) result += ' ' + std::string(g_scales[scale]);
        }
        pos += glen;
    }
    return result;
}

// 1 = ASCII letter (English), 2 = CJK lead byte (Chinese), 0 = no script.
unsigned char script_of(unsigned char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;
    if (c >= 0xE4 && c <= 0xE9) return 2;
    return 0;
}

// Script of the first ASCII letter or CJK character in the text, 0 if none.
unsigned char first_script(const std::string& text) {
    for (const char c : text) {
        const unsigned char script = script_of(static_cast<unsigned char>(c));
        if (script != 0) return script;
    }
    return 0;
}

// "one nine two dot one six eight dot zero dot one" for "192.168.0.1".
std::string dotted_digits_to_english(const std::string& digits) {
    std::string result;
    size_t seg_start = 0;
    for (size_t i = 0; i <= digits.size(); ++i) {
        if (i == digits.size() || digits[i] == '.') {
            if (!result.empty()) result += " dot ";
            result += spell_digits(digits.substr(seg_start, i - seg_start));
            seg_start = i + 1;
        }
    }
    return result;
}

bool pad_before(const std::string& result) {
    if (result.empty()) return false;
    const unsigned char last = result.back();
    return (last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z') || (last >= '0' && last <= '9');
}

bool pad_after(const std::string& text, size_t match_end) {
    if (match_end >= text.size()) return false;
    const unsigned char next = static_cast<unsigned char>(text[match_end]);
    return (next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z') || (next >= '0' && next <= '9');
}

}  // namespace

std::string number_to_english(const std::string& num_str) {
    size_t pos = 0;
    std::string prefix;
    if (!num_str.empty() && num_str[0] == '-') {
        prefix = "minus ";
        pos = 1;
    } else if (!num_str.empty() && num_str[0] == '+') {
        pos = 1;
    }
    const std::string digits = num_str.substr(pos);

    int dot_count = 0;
    for (const char c : digits) {
        if (c == '.') ++dot_count;
    }
    if (dot_count > 1) return prefix + dotted_digits_to_english(digits);

    const size_t dot = digits.find('.');
    const std::string integer_part = dot == std::string::npos ? digits : digits.substr(0, dot);
    std::string result = prefix + integer_to_english(integer_part);
    if (dot != std::string::npos) result += " point " + spell_digits(digits.substr(dot + 1));
    return result;
}

std::string normalize_numbers(const std::string& text, NumberLanguage language) {
    static const std::regex num_regex("[-+]?\\d+(?:\\.\\d+)*");
    std::string result;
    size_t last_pos = 0;
    unsigned char last_script = 0;
    const unsigned char text_script = first_script(text);

    for (std::sregex_iterator it = std::sregex_iterator(text.begin(), text.end(), num_regex), end = std::sregex_iterator();
         it != end; ++it) {
        const std::smatch match = *it;
        const std::string gap = text.substr(last_pos, match.position() - last_pos);
        for (const char c : gap) {
            const unsigned char script = script_of(static_cast<unsigned char>(c));
            if (script != 0) last_script = script;
        }
        const unsigned char resolved = last_script != 0 ? last_script : text_script;
        const bool english = language == NumberLanguage::English || (language == NumberLanguage::Auto && resolved == 1);
        const size_t match_end = match.position() + match.length();
        std::string replacement = english ? number_to_english(match.str()) : BasicStringUtil::NumberToChinese(match.str());
        result += gap;
        if (english) {
            if (pad_before(result)) replacement = " " + replacement;
            if (pad_after(text, match_end)) replacement += " ";
        }
        result += replacement;
        last_pos = match_end;
    }
    if (last_pos < text.size()) result += text.substr(last_pos);
    return result;
}
