#include "Phonemizer.h"

#include "chinese_english/ChineseEnglishPhonemizer.h"
#include "chinese_english/JiebaProcessor.h"
#include "english/EnglishPhonemizer.h"
#include "spanish/SpanishPhonemizer.h"

namespace {

std::string with_trailing_slash(std::string dir) {
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += "/";
    return dir;
}

}  // namespace

Phonemizer::Phonemizer(const PhonemizerConfig& config) {
    const std::string d = with_trailing_slash(config.dict_dir);
    auto eng = std::make_shared<const EnG2P>(d + config.cmu_dict, d + config.user_en_dict, d + config.g2p_en_model);
    auto jieba = std::make_shared<JiebaProcessor>(d + config.jieba_dict, d + config.hmm_model, d + config.user_dict,
                                                  d + config.idf_path, d + config.stop_word_path,
                                                  d + config.pinyin_char, d + config.pinyin_phrase);
    phonemizers_.emplace(G2PLanguage::ChineseEnglish, std::make_unique<ChineseEnglishPhonemizer>(jieba, eng));
    phonemizers_.emplace(G2PLanguage::English, std::make_unique<EnglishPhonemizer>(eng));
    phonemizers_.emplace(G2PLanguage::Spanish,
                         std::make_unique<SpanishPhonemizer>(SpanishLoanwords::load(d + config.es_loanwords)));
}

Phonemizer::~Phonemizer() = default;

std::string Phonemizer::phonemize(const std::string& text, G2PLanguage language) {
    return phonemizers_.at(language)->phonemize(text, number_language_);
}
