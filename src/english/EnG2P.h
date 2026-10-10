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

    std::string convert(const std::string& word) const {
        size_t start = 0;
        while (start < word.size() && !isalnum(static_cast<unsigned char>(word[start]))) start++;
        size_t end = word.size();
        while (end > start && !isalnum(static_cast<unsigned char>(word[end - 1]))) end--;
        const std::vector<std::string> phonemes = lookup(word.substr(start, end - start));
        if (phonemes.empty()) return word;
        return word.substr(0, start) + arpabet_to_ipa(phonemes) + word.substr(end);
    }

private:
    // word: trimmed of surrounding punctuation, original case. Empty result: no pronunciation.
    std::vector<std::string> lookup(const std::string& word) const {
        std::string upper = word;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        const auto it = dict_.find(upper);
        if (it != dict_.end()) return it->second;
        if (upper.empty()) return {};
        if (auto possessive = lookup_possessive(word); !possessive.empty()) return possessive;

        const auto is_upper = [](unsigned char c) { return c >= 'A' && c <= 'Z'; };
        const bool letters_only = std::all_of(upper.begin(), upper.end(), is_upper);
        // Short all-caps words (GPU, API) are acronyms: spell them. Also the fallback
        // for any unknown word when no neural model is loaded.
        const bool acronym = letters_only && word.size() <= MAX_ACRONYM_LENGTH &&
                             std::all_of(word.begin(), word.end(), is_upper);
        if (letters_only && (acronym || !neural_.loaded())) return spell(upper);

        const bool word_chars = std::all_of(upper.begin(), upper.end(),
                                            [&](unsigned char c) { return is_upper(c) || c == '\''; });
        if (!word_chars || !neural_.loaded()) return {};
        std::string lower = upper;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return neural_.predict(lower);
    }

    // "Pedro's", "GPU's" missing from the dicts: stem pronunciation plus the English -s suffix.
    std::vector<std::string> lookup_possessive(const std::string& word) const {
        const size_t n = word.size();
        if (n <= 2 || word[n - 2] != '\'' || (word[n - 1] != 's' && word[n - 1] != 'S')) return {};
        std::vector<std::string> phonemes = lookup(word.substr(0, n - 2));
        if (phonemes.empty()) return {};
        const std::vector<std::string> suffix = s_suffix(phonemes.back());
        phonemes.insert(phonemes.end(), suffix.begin(), suffix.end());
        return phonemes;
    }

    // -s allomorph: ɪz after sibilants, s after voiceless consonants, z otherwise.
    static std::vector<std::string> s_suffix(std::string last) {
        if (!last.empty() && isdigit(static_cast<unsigned char>(last.back()))) last.pop_back();
        static const std::unordered_set<std::string> SIBILANT = {"S", "Z", "SH", "ZH", "CH", "JH"};
        static const std::unordered_set<std::string> VOICELESS = {"P", "T", "K", "F", "TH"};
        if (SIBILANT.count(last)) return {"IH0", "Z"};
        if (VOICELESS.count(last)) return {"S"};
        return {"Z"};
    }

    static std::vector<std::string> spell(const std::string& upper) {
        std::vector<std::string> phonemes;
        for (char c : upper) {
            const auto& letter = LETTER_NAMES[c - 'A'];
            phonemes.insert(phonemes.end(), letter.begin(), letter.end());
        }
        return phonemes;
    }

    std::unordered_map<std::string, std::vector<std::string>> dict_;
    NeuralG2P neural_{neural_symbols()};
    static constexpr size_t MAX_ACRONYM_LENGTH = 5;

    // Symbol tables from g2p_en/g2p.py; indices must match the trained embeddings.
    static G2PSymbols neural_symbols() {
        G2PSymbols symbols;
        symbols.graphemes = {"<pad>", "<unk>", "</s>"};
        for (char c = 'a'; c <= 'z'; ++c) symbols.graphemes.emplace_back(1, c);
        symbols.phonemes = {
            "<pad>", "<unk>", "<s>", "</s>",
            "AA0", "AA1", "AA2", "AE0", "AE1", "AE2", "AH0", "AH1", "AH2", "AO0",
            "AO1", "AO2", "AW0", "AW1", "AW2", "AY0", "AY1", "AY2", "B", "CH", "D", "DH",
            "EH0", "EH1", "EH2", "ER0", "ER1", "ER2", "EY0", "EY1", "EY2", "F", "G", "HH",
            "IH0", "IH1", "IH2", "IY0", "IY1", "IY2", "JH", "K", "L",
            "M", "N", "NG", "OW0", "OW1", "OW2", "OY0", "OY1", "OY2", "P", "R", "S", "SH", "T", "TH",
            "UH0", "UH1", "UH2", "UW", "UW0", "UW1", "UW2", "V", "W", "Y", "Z", "ZH",
        };
        return symbols;
    }

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
    // exist in dict/vocab.txt: PhonemeEncoder::encode silently drops unknown symbols.
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
