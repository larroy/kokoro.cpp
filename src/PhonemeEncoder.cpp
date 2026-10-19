#include "PhonemeEncoder.h"
#include "Utf8.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

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
