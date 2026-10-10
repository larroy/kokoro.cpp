#pragma once
#include <string>
#include "NumberNormalizer.h"

// Text -> phoneme string in Kokoro's phoneme set, for one language.
class PhonemizerBase {
public:
    virtual ~PhonemizerBase() = default;
    // text: UTF-8. numbers: how digits are read; Auto applies the implementation's own rule.
    virtual std::string phonemize(const std::string& text, NumberLanguage numbers) = 0;
};
