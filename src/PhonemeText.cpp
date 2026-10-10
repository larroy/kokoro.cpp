#include "PhonemeText.h"

namespace {

const std::u16string kNoSpaceBefore = u",.;:!?…)”";
const std::u16string kNoSpaceAfter = u"(“¿¡";

bool contains(const std::u16string& set, char16_t c) { return set.find(c) != std::u16string::npos; }

}  // namespace

std::u16string tidy_spaces(const std::u16string& text) {
    std::u16string out;
    for (size_t i = 0; i < text.size(); ++i) {
        const char16_t c = text[i];
        const bool drop = c == u' ' && (out.empty() || out.back() == u' ' || contains(kNoSpaceAfter, out.back()) ||
                                        i + 1 == text.size() || contains(kNoSpaceBefore, text[i + 1]));
        if (!drop) out += c;
    }
    while (!out.empty() && out.back() == u' ') out.pop_back();
    return out;
}
