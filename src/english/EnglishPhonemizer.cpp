#include "EnglishPhonemizer.h"

#include <cctype>
#include <utility>

namespace {

// ASCII letters, digits and the apostrophe form words ("rock'n'roll", "GPU's").
bool is_word_byte(char ch) {
    const auto c = static_cast<unsigned char>(ch);
    return (c < 0x80 && std::isalnum(c)) || c == '\'';
}

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    for (size_t pos = str.find(from); pos != std::string::npos; pos = str.find(from, pos + to.size())) {
        str.replace(pos, from.size(), to);
    }
    return str;
}

std::string trim(const std::string& str) {
    const char* blanks = " \t\n\r";
    const size_t first = str.find_first_not_of(blanks);
    if (first == std::string::npos) return "";
    return str.substr(first, str.find_last_not_of(blanks) - first + 1);
}

}  // namespace

EnglishPhonemizer::EnglishPhonemizer(std::shared_ptr<const EnG2P> g2p) : g2p_(std::move(g2p)) {}

std::string EnglishPhonemizer::phonemize(const std::string& text, NumberLanguage numbers) {
    const NumberLanguage language = numbers == NumberLanguage::Auto ? NumberLanguage::English : numbers;
    // Curly apostrophe -> ASCII, as ZHG2P::map_punctuation does, so "It’s" stays one word.
    const std::string normalized = trim(replace_all(normalize_numbers(text, language), "\xE2\x80\x99", "'"));
    std::string result;
    for (size_t i = 0; i < normalized.size();) {
        if (!is_word_byte(normalized[i])) {
            result += normalized[i++];
            continue;
        }
        const size_t start = i;
        while (i < normalized.size() && is_word_byte(normalized[i])) ++i;
        result += g2p_->convert(normalized.substr(start, i - start));
    }
    return result;
}
