#include "PhonemeChunker.h"

#include <regex>

std::vector<std::string> split_phonemes(const std::string& phonemes, std::size_t max_length) {
    std::vector<std::string> batches;
    std::regex re("([.,!?;])");
    std::sregex_token_iterator it(phonemes.begin(), phonemes.end(), re, {-1, 0}); // -1 for non-match, 0 for match
    std::sregex_token_iterator end;

    std::string current_batch;
    
    for (; it != end; ++it) {
        std::string part = *it;
        // Removing leading/trailing whitespace
        part = std::regex_replace(part, std::regex("^\\s+|\\s+$"), "");
        
        if (part.empty()) continue;

        if (current_batch.length() + part.length() + 1 >= max_length) {
            if (!current_batch.empty()) batches.push_back(current_batch);
            current_batch = part;
        } else {
             if (std::string(".,!?;").find(part) != std::string::npos) {
                current_batch += part;
             } else {
                if (!current_batch.empty()) current_batch += " ";
                current_batch += part;
             }
        }
    }
    if (!current_batch.empty()) {
        batches.push_back(current_batch);
    }
    return batches;
}
