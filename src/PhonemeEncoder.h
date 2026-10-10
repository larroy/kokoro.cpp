#pragma once
#include <map>
#include <string>
#include <vector>

// Phoneme string -> model token ids. Language-agnostic: one id per UTF-8 character found in the vocab.
class PhonemeEncoder {
public:
    // Reads `token<TAB>id` lines; \n, \r, \t escapes in tokens are unescaped; lines without a tab or with a
    // non-integer id are skipped. Throws std::runtime_error if the file cannot be opened or yields no tokens.
    static PhonemeEncoder load(const std::string& vocab_path);
    explicit PhonemeEncoder(std::map<std::string, int> vocab);
    // Characters missing from the vocab are dropped.
    std::vector<int> encode(const std::string& phonemes) const;

private:
    std::map<std::string, int> vocab_;
};
