// Prints one phoneme line per stdin line, for compare_g2p.py.
#include "Phonemizer.h"
#include "german/GermanG2P.h"
#include "spanish/SpanishG2P.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {

using LineG2P = std::function<std::string(const std::string&)>;

// G2P for `language` reading dictionaries from dict_dir; empty for an unknown language.
LineG2P make_g2p(const std::string& language, const std::string& dict_dir) {
    if (language == "es") {
        return [](const std::string& line) { return spanish_to_phonemes(line, NumberLanguage::Auto); };
    }
    if (language != "de") return {};
    const PhonemizerConfig config;
    auto lexicon = std::make_shared<const GermanLexicon>(dict_dir + "/" + config.de_dict,
                                                         dict_dir + "/" + config.g2p_de_model);
    return [lexicon](const std::string& line) { return german_to_phonemes(line, NumberLanguage::Auto, *lexicon); };
}

}  // namespace

int main(int argc, char** argv) {
    const LineG2P g2p = argc == 2 || argc == 3 ? make_g2p(argv[1], argc == 3 ? argv[2] : "dict") : LineG2P{};
    if (!g2p) {
        std::cerr << "usage: g2p_dump <language> [dict_dir]\n  language: es, de\n";
        return 2;
    }
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::cout << g2p(line) << '\n';
    }
    return 0;
}
