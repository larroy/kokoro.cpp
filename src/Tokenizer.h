#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "G2PLanguage.h"
#include "NumberNormalizer.h"
#include "SpanishLoanwords.h"

class ZHG2P;
class JiebaProcessor;

struct TokenizerConfig {
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
};

class Tokenizer {
public:
    Tokenizer(const TokenizerConfig& config = {}, const std::map<std::string, int>& vocab = {});
    ~Tokenizer();
    
    std::vector<int> tokenize(const std::string& phonemes);
    // Phonemes for text, read by the G2P for `language`.
    std::string phonemize(const std::string& text, G2PLanguage language);

    // How G2P reads digits in later phonemize() calls, for both G2Ps.
    void set_number_language(NumberLanguage language);

private:
    std::map<std::string, int> vocab_;
    std::shared_ptr<JiebaProcessor> processor_;
    std::unique_ptr<ZHG2P> g2p_;
    SpanishLoanwords spanish_loanwords_;
    NumberLanguage number_language_ = NumberLanguage::Auto;
};
