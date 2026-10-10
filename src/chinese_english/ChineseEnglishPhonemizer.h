#pragma once
#include <memory>
#include <string>
#include "PhonemizerBase.h"
#include "ZHG2P.h"

// Mixed Chinese/English text -> phonemes via ZHG2P. Auto numbers follow the nearest script.
class ChineseEnglishPhonemizer : public PhonemizerBase {
public:
    ChineseEnglishPhonemizer(std::shared_ptr<TextProcessor> processor, std::shared_ptr<const EnG2P> eng_g2p);
    std::string phonemize(const std::string& text, NumberLanguage numbers) override;

private:
    ZHG2P g2p_;
};
