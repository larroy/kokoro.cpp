#pragma once
#include <memory>
#include <string>
#include "PhonemizerBase.h"
#include "EnG2P.h"

// English text -> misaki phonemes via EnG2P. Auto numbers read as English; non-word characters pass through
// unchanged (PhonemeEncoder drops those outside the vocab).
class EnglishPhonemizer : public PhonemizerBase {
public:
    explicit EnglishPhonemizer(std::shared_ptr<const EnG2P> g2p);
    std::string phonemize(const std::string& text, NumberLanguage numbers) override;

private:
    std::shared_ptr<const EnG2P> g2p_;
};
