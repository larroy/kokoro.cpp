#pragma once

#include <optional>
#include <string>
#include <unordered_map>

// ASCII and Spanish capitals (ÁÉÍÓÚÜÑ) to lowercase. Other characters are unchanged.
char16_t spanish_to_lower(char16_t c);

// Grave-accented vowels (àèìòù, either case) to the plain vowel of the same case, as Spanish has no grave accent
// ("déjà vu", "à la carte"). Other characters are unchanged.
char16_t fold_grave(char16_t c);

// Loanwords Spanish speakers pronounce differently from their spelling, mapped to a lowercase respelling in Spanish
// orthography that the Spanish G2P reads instead: a written accent marks stress, and IPA consonants missing from
// Spanish spelling are written directly ("show" -> "ʃóu").
class SpanishLoanwords {
public:
    // The compiled-in table only.
    static const SpanishLoanwords& builtin();
    // The compiled-in table merged with the TSV at tsv_path, when that file exists. TSV entries override built-ins.
    // Format: word<TAB>respelling per line; blank lines and lines starting with # are ignored.
    static SpanishLoanwords load(const std::string& tsv_path);

    // Respelling for a lowercase word, or for the plural -s of a listed word ("parkings" -> "párkins").
    std::optional<std::u16string> respell(const std::u16string& word) const;

private:
    std::unordered_map<std::u16string, std::u16string> entries_;
};
