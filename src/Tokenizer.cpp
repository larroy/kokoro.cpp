#include "Tokenizer.h"
#include "JiebaProcessor.h"
#include "ZHG2P.h"
#include <iostream>
#include <vector>

Tokenizer::Tokenizer(const TokenizerConfig& config, const std::map<std::string, int>& vocab) 
    : vocab_(vocab) {
    
    std::string d = config.dict_dir;
    if (!d.empty() && d.back() != '/') d += "/";
    
    std::string jieba_dict = d + config.jieba_dict;
    std::string hmm_model = d + config.hmm_model;
    std::string user_dict = d + config.user_dict;
    std::string idf_path = d + config.idf_path;
    std::string stop_word_path = d + config.stop_word_path;
    std::string pinyin_char = d + config.pinyin_char;
    std::string pinyin_phrase = d + config.pinyin_phrase;
    std::string cmu_dict = d + config.cmu_dict;
    std::string user_en_dict = d + config.user_en_dict;
    std::string g2p_en_model = d + config.g2p_en_model;

    processor_ = std::make_shared<JiebaProcessor>(
        jieba_dict, hmm_model, user_dict, idf_path, stop_word_path, pinyin_char, pinyin_phrase
    );
    g2p_ = std::make_unique<ZHG2P>(processor_, "1.1", "<unk>", cmu_dict, user_en_dict, g2p_en_model);
}

Tokenizer::~Tokenizer() = default;

static std::vector<std::string> split_utf8(const std::string& str) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t char_len = 0;
        if (c < 0x80) char_len = 1;
        else if ((c & 0xE0) == 0xC0) char_len = 2;
        else if ((c & 0xF0) == 0xE0) char_len = 3;
        else if ((c & 0xF8) == 0xF0) char_len = 4;
        else char_len = 1; 

        if (i + char_len > str.length()) char_len = str.length() - i;
        
        chars.push_back(str.substr(i, char_len));
        i += char_len;
    }
    return chars;
}

std::vector<int> Tokenizer::tokenize(const std::string& phonemes) {
    std::vector<int> tokens;
    std::vector<std::string> chars = split_utf8(phonemes);
    
    for (const auto& c : chars) {
        if (vocab_.count(c)) {
            tokens.push_back(vocab_.at(c));
        }
    }
    return tokens;
}

std::string Tokenizer::phonemize(const std::string& text, bool norm) {
    auto result = (*g2p_)(text);
    return result.first;
}

void Tokenizer::set_number_language(NumberLanguage language) {
    if (g2p_) g2p_->set_number_language(language);
}
