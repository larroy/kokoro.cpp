#pragma once
#include <map>
#include <memory>
#include <string>
#include "G2PLanguage.h"
#include "NumberNormalizer.h"

class PhonemizerBase;

// Dictionary file names, relative to dict_dir.
struct PhonemizerConfig {
    std::string dict_dir = "dict/";
    std::string jieba_dict = "jieba.dict.utf8";
    std::string hmm_model = "hmm_model.utf8";
    std::string user_dict = "user.dict.utf8";
    std::string idf_path = "idf.utf8";
    std::string stop_word_path = "stop_words.utf8";
    std::string pinyin_char = "pinyin.txt";
    std::string pinyin_phrase = "pinyin_phrase.txt";
    std::string cmu_dict = "cmudict-0.7b/cmudict.dict";
    std::string user_en_dict = "user_en.dict";
    std::string g2p_en_model = "g2p_en.weights";
    std::string es_loanwords = "es_loanwords.tsv";  // optional; extends the built-in Spanish loanwords
    std::string de_dict = "german_mfa.dict";
    std::string g2p_de_model = "g2p_de.weights";
};

// Dispatches text to the PhonemizerBase implementation for a G2PLanguage.
class Phonemizer {
public:
    explicit Phonemizer(const PhonemizerConfig& config);
    ~Phonemizer();
    std::string phonemize(const std::string& text, G2PLanguage language);
    // How digits are read by later phonemize() calls, for every language.
    void set_number_language(NumberLanguage language) { number_language_ = language; }

private:
    std::map<G2PLanguage, std::unique_ptr<PhonemizerBase>> phonemizers_;
    NumberLanguage number_language_ = NumberLanguage::Auto;
};
