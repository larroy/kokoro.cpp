#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "NeuralG2P.h"

class EnG2P {
public:
    // user_dict_path: optional CMU-format lexicon whose entries override the CMU dict.
    // neural_model_path: optional g2p_en weights used to predict words missing from both dicts.
    EnG2P(const std::string& dict_path, const std::string& user_dict_path = "",
          const std::string& neural_model_path = "") {
        if (!dict_path.empty()) load_dict(dict_path, false);
        if (!user_dict_path.empty()) load_dict(user_dict_path, true);
        if (!neural_model_path.empty()) neural_.load(neural_model_path);
    }

    std::string convert(const std::string& word) {
        std::string upper_word = word;
        // Simple strip of punctuation if needed? 
        // But let's just assume the input is somewhat clean or strict match first.
        
        // Trim common punctuation from ends just in case
        size_t start = 0;
        while (start < upper_word.size() && !isalnum((unsigned char)upper_word[start])) start++;
        size_t end = upper_word.size();
        while (end > start && !isalnum((unsigned char)upper_word[end-1])) end--;
        
        std::string clean_word = upper_word.substr(start, end - start);
        std::string prefix = upper_word.substr(0, start);
        std::string suffix = upper_word.substr(end);
        
        const std::string original = clean_word;
        std::transform(clean_word.begin(), clean_word.end(), clean_word.begin(), ::toupper);
        
        // std::cout << "Debug EnG2P: Query [" << clean_word << "]" << std::endl;
        
        auto it = dict_.find(clean_word);
        if (it != dict_.end()) {
            return prefix + arpabet_to_ipa(it->second) + suffix;
        }

        if (clean_word.empty()) return word;
        const auto is_upper = [](unsigned char c) { return c >= 'A' && c <= 'Z'; };
        const bool letters_only = std::all_of(clean_word.begin(), clean_word.end(), is_upper);

        // Short all-caps words (GPU, API) are acronyms: spell them. Also the fallback
        // for any unknown word when no neural model is loaded.
        const bool acronym = letters_only && original.size() <= MAX_ACRONYM_LENGTH &&
                             std::all_of(original.begin(), original.end(), is_upper);
        if (letters_only && (acronym || !neural_.loaded())) {
            std::string spelled;
            for (char c : clean_word) spelled += arpabet_to_ipa(LETTER_NAMES[c - 'A']);
            return prefix + spelled + suffix;
        }

        const bool word_chars = std::all_of(clean_word.begin(), clean_word.end(),
                                            [&](unsigned char c) { return is_upper(c) || c == '\''; });
        if (word_chars && neural_.loaded()) {
            std::string lower = clean_word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return prefix + arpabet_to_ipa(neural_.predict(lower)) + suffix;
        }

        return word;
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> dict_;
    NeuralG2P neural_;
    static constexpr size_t MAX_ACRONYM_LENGTH = 5;

    // English letter names in ARPAbet, A..Z.
    inline static const std::vector<std::string> LETTER_NAMES[26] = {
        {"EY1"}, {"B", "IY1"}, {"S", "IY1"}, {"D", "IY1"}, {"IY1"},
        {"EH1", "F"}, {"JH", "IY1"}, {"EY1", "CH"}, {"AY1"}, {"JH", "EY1"},
        {"K", "EY1"}, {"EH1", "L"}, {"EH1", "M"}, {"EH1", "N"}, {"OW1"},
        {"P", "IY1"}, {"K", "Y", "UW1"}, {"AA1", "R"}, {"EH1", "S"}, {"T", "IY1"},
        {"Y", "UW1"}, {"V", "IY1"}, {"D", "AH1", "B", "AH0", "L", "Y", "UW0"}, {"EH1", "K", "S"}, {"W", "AY1"},
        {"Z", "IY1"}
    };

    // override=false: first variant wins and existing entries are kept (CMU dict).
    // override=true: entries replace existing ones; first variant in this file wins.
    void load_dict(const std::string& path, bool override) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[EnG2P] Warning: Failed to open dict: " << path << std::endl;
            return;
        }
        std::unordered_set<std::string> seen;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            // CMU dict lines start with word, possibly with symbols like !EXCLAMATION-POINT
            // Standard format: WORD  PH ON E M ES
            if (!isalpha(line[0]) && line[0] != '\'') continue; // Basic filtering

            std::stringstream ss(line);
            std::string word, ph;
            ss >> word;
            
            // Handle variants like WORD(1)
            size_t paren = word.find('(');
            if (paren != std::string::npos) {
                word = word.substr(0, paren);
            }

            // Normalize to UPPERCASE
            std::transform(word.begin(), word.end(), word.begin(), ::toupper);

            std::vector<std::string> phonemes;
            while (ss >> ph) {
                phonemes.push_back(ph);
            }
            
            if (override) {
                if (seen.insert(word).second) dict_[word] = std::move(phonemes);
            } else if (!dict_.count(word)) {
                dict_[word] = std::move(phonemes);
            }
        }
    }

    // ARPAbet -> Kokoro's English phoneme set (misaki, US; see
    // https://github.com/hexgrad/misaki/blob/main/EN_PHONES.md). Every output symbol must
    // exist in dict/vocab.txt: Tokenizer::tokenize silently drops unknown symbols.
    // Diphthongs use misaki's single-letter forms: A=eɪ, I=aɪ, O=oʊ, W=aʊ, Y=ɔɪ.
    static std::string arpabet_to_ipa(const std::vector<std::string>& phonemes) {
        static const std::unordered_map<std::string, const char*> MISAKI = {
            {"AA", "ɑ"}, {"AE", "æ"}, {"AH", "ʌ"}, {"AO", "ɔ"}, {"AW", "W"}, {"AY", "I"},
            {"B", "b"}, {"CH", "ʧ"}, {"D", "d"}, {"DH", "ð"}, {"EH", "ɛ"}, {"ER", "ɜɹ"},
            {"EY", "A"}, {"F", "f"}, {"G", "ɡ"}, {"HH", "h"}, {"IH", "ɪ"}, {"IY", "i"},
            {"JH", "ʤ"}, {"K", "k"}, {"L", "l"}, {"M", "m"}, {"N", "n"}, {"NG", "ŋ"},
            {"OW", "O"}, {"OY", "Y"}, {"P", "p"}, {"R", "ɹ"}, {"S", "s"}, {"SH", "ʃ"},
            {"T", "t"}, {"TH", "θ"}, {"UH", "ʊ"}, {"UW", "u"}, {"V", "v"}, {"W", "w"},
            {"Y", "j"}, {"Z", "z"}, {"ZH", "ʒ"},
        };
        std::string res;
        for (const auto& p : phonemes) {
            std::string base = p;
            char stress = 0;
            if (!base.empty() && isdigit(static_cast<unsigned char>(base.back()))) {
                stress = base.back();
                base.pop_back();
            }
            auto it = MISAKI.find(base);
            if (it == MISAKI.end()) continue;
            if (stress == '1') res += "ˈ";
            else if (stress == '2') res += "ˌ";
            // Unstressed AH/ER reduce to schwa: "about" əbˈWt, "butter" bˈʌtəɹ.
            if (stress == '0' && base == "AH") res += "ə";
            else if (stress == '0' && base == "ER") res += "əɹ";
            else res += it->second;
        }
        return res;
    }
};
