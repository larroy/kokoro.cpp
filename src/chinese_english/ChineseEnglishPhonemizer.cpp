#include "ChineseEnglishPhonemizer.h"

#include <utility>

ChineseEnglishPhonemizer::ChineseEnglishPhonemizer(std::shared_ptr<TextProcessor> processor,
                                                   std::shared_ptr<const EnG2P> eng_g2p)
    : g2p_(std::move(processor), std::move(eng_g2p)) {}

std::string ChineseEnglishPhonemizer::phonemize(const std::string& text, NumberLanguage numbers) {
    g2p_.set_number_language(numbers);
    return g2p_(text).first;
}
