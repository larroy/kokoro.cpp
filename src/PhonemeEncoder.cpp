#include "PhonemeEncoder.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

std::vector<std::string> split_utf8(const std::string& str) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length();) {
        const auto c = static_cast<unsigned char>(str[i]);
        size_t char_len = 1;
        if ((c & 0xE0) == 0xC0) char_len = 2;
        else if ((c & 0xF0) == 0xE0) char_len = 3;
        else if ((c & 0xF8) == 0xF0) char_len = 4;
        if (i + char_len > str.length()) char_len = str.length() - i;
        chars.push_back(str.substr(i, char_len));
        i += char_len;
    }
    return chars;
}

void unescape(std::string& token, const std::string& escape, const char* plain) {
    for (size_t pos = token.find(escape); pos != std::string::npos; pos = token.find(escape, pos + 1)) {
        token.replace(pos, escape.size(), plain);
    }
}

}  // namespace

PhonemeEncoder PhonemeEncoder::load(const std::string& vocab_path) {
    std::ifstream in(std::filesystem::u8path(vocab_path));
    if (!in.is_open()) throw std::runtime_error("Failed to open vocab file: " + vocab_path);
    std::map<std::string, int> vocab;
    std::string line;
    while (std::getline(in, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string token = line.substr(0, tab);
        unescape(token, "\\n", "\n");
        unescape(token, "\\r", "\r");
        unescape(token, "\\t", "\t");
        try {
            vocab[token] = std::stoi(line.substr(tab + 1));
        } catch (...) {}
    }
    if (vocab.empty()) throw std::runtime_error("Vocab file contains no tokens: " + vocab_path);
    return PhonemeEncoder(std::move(vocab));
}

PhonemeEncoder::PhonemeEncoder(std::map<std::string, int> vocab) : vocab_(std::move(vocab)) {}

std::vector<int> PhonemeEncoder::encode(const std::string& phonemes) const {
    std::vector<int> tokens;
    for (const auto& c : split_utf8(phonemes)) {
        if (const auto it = vocab_.find(c); it != vocab_.end()) tokens.push_back(it->second);
    }
    return tokens;
}
