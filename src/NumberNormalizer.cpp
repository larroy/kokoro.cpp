#include "NumberNormalizer.h"

#include "Utils.h"

#include <regex>
#include <utility>

namespace {

const char* const g_ones_to_nineteen[] = {"",        "one",     "two",       "three",    "four",
                                          "five",    "six",     "seven",     "eight",    "nine",
                                          "ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                                          "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
const char* const g_tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* const g_digit_words[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
const char* const g_scales[] = {"", "thousand", "million", "billion", "trillion"};

const char* const g_es_below_thirty[] = {
    "cero",       "uno",        "dos",        "tres",         "cuatro",      "cinco",       "seis",
    "siete",      "ocho",       "nueve",      "diez",         "once",        "doce",        "trece",
    "catorce",    "quince",     "dieciséis",  "diecisiete",   "dieciocho",   "diecinueve",  "veinte",
    "veintiuno",  "veintidós",  "veintitrés", "veinticuatro", "veinticinco", "veintiséis",  "veintisiete",
    "veintiocho", "veintinueve"};
const char* const g_es_tens[] = {"", "", "", "treinta", "cuarenta", "cincuenta", "sesenta", "setenta", "ochenta",
                                 "noventa"};
const char* const g_es_hundreds[] = {"",           "ciento",     "doscientos",  "trescientos", "cuatrocientos",
                                     "quinientos", "seiscientos", "setecientos", "ochocientos", "novecientos"};

// Words a reader puts around the digits of one number match.
struct NumberWords {
    const char* minus;
    const char* point;  // between the integer and the fraction
    const char* dot;    // between segments of a number with several dots
    const char* const* digits;
    std::string (*integer)(const std::string& digits);
};

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
std::string spell_digits(const std::string& digits, const char* const* words = g_digit_words) {
    std::string result;
    for (const char c : digits) {
        if (!result.empty()) result += ' ';
        result += words[c - '0'];
    }
    return result;
}

// Spanish words for 0 < n < 100; apocope: "un"/"veintiún" before mil/millón.
std::string es_below_hundred(int n, bool apocope) {
    if (apocope && n == 1) return "un";
    if (apocope && n == 21) return "veintiún";
    if (n < 30) return g_es_below_thirty[n];
    const std::string tens = g_es_tens[n / 10];
    const int ones = n % 10;
    if (ones == 0) return tens;
    return tens + " y " + (apocope && ones == 1 ? "un" : g_es_below_thirty[ones]);
}

// Spanish words for 0 < n < 1000: "ciento uno", "cien".
std::string es_below_thousand(int n, bool apocope) {
    if (n == 100) return "cien";
    const int rest = n % 100;
    if (n < 100) return es_below_hundred(rest, apocope);
    const std::string hundreds = g_es_hundreds[n / 100];
    return rest == 0 ? hundreds : hundreds + " " + es_below_hundred(rest, apocope);
}

// Spanish words for 0 < n < 1000000: "veintiún mil quinientos".
std::string es_below_million(int n, bool apocope) {
    const int thousands = n / 1000;
    const int rest = n % 1000;
    const std::string head = thousands == 0 ? "" : thousands == 1 ? "mil" : es_below_thousand(thousands, true) + " mil";
    if (rest == 0) return head;
    return head.empty() ? es_below_thousand(rest, apocope) : head + " " + es_below_thousand(rest, apocope);
}

// Spanish words for an integer digit string: "2500000" -> "dos millones quinientos mil".
std::string integer_to_spanish(const std::string& digits) {
    const size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) return "cero";
    const std::string trimmed = digits.substr(first);
    if (trimmed.size() > 12) return spell_digits(trimmed, g_es_below_thirty);
    const long long value = std::stoll(trimmed);
    const int millions = static_cast<int>(value / 1000000);
    const int rest = static_cast<int>(value % 1000000);
    const std::string head = millions == 0   ? ""
                             : millions == 1 ? "un millón"
                                             : es_below_million(millions, true) + " millones";
    if (rest == 0) return head;
    return head.empty() ? es_below_million(rest, false) : head + " " + es_below_million(rest, false);
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
std::string dotted_digits(const std::string& digits, const NumberWords& words) {
    std::string result;
    size_t seg_start = 0;
    for (size_t i = 0; i <= digits.size(); ++i) {
        if (i == digits.size() || digits[i] == '.') {
            if (!result.empty()) result += words.dot;
            result += spell_digits(digits.substr(seg_start, i - seg_start), words.digits);
            seg_start = i + 1;
        }
    }
    return result;
}

// Reads one number match: sign, integer part, then a fraction or dotted segments.
std::string read_number(const std::string& num_str, const NumberWords& words) {
    const bool has_sign = !num_str.empty() && (num_str[0] == '-' || num_str[0] == '+');
    const std::string prefix = has_sign && num_str[0] == '-' ? words.minus : "";
    const std::string digits = num_str.substr(has_sign ? 1 : 0);
    const size_t dot = digits.find('.');
    if (dot != std::string::npos && digits.find('.', dot + 1) != std::string::npos) {
        return prefix + dotted_digits(digits, words);
    }
    std::string result = prefix + words.integer(digits.substr(0, dot));
    if (dot != std::string::npos) result += words.point + spell_digits(digits.substr(dot + 1), words.digits);
    return result;
}

const NumberWords g_english_words{"minus ", " point ", " dot ", g_digit_words, integer_to_english};
const NumberWords g_spanish_words{"menos ", " punto ", " punto ", g_es_below_thirty, integer_to_spanish};

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

// Unpadded words for one match, and whether they need spaces against adjacent letters/digits.
std::pair<std::string, bool> spoken_number(const std::string& match, NumberLanguage language,
                                           unsigned char resolved_script) {
    if (language == NumberLanguage::Spanish) return {read_number(match, g_spanish_words), true};
    const bool english = language == NumberLanguage::English || (language == NumberLanguage::Auto && resolved_script == 1);
    if (english) return {read_number(match, g_english_words), true};
    return {BasicStringUtil::NumberToChinese(match), false};
}

}  // namespace

std::string number_to_english(const std::string& num_str) { return read_number(num_str, g_english_words); }

std::string number_to_spanish(const std::string& num_str) { return read_number(num_str, g_spanish_words); }

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
        const size_t match_end = match.position() + match.length();
        auto [replacement, pads] = spoken_number(match.str(), language, last_script != 0 ? last_script : text_script);
        result += gap;
        if (pads && pad_before(result)) replacement = " " + replacement;
        if (pads && pad_after(text, match_end)) replacement += " ";
        result += replacement;
        last_pos = match_end;
    }
    if (last_pos < text.size()) result += text.substr(last_pos);
    return result;
}
