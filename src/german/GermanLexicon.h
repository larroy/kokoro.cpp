#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "NeuralG2P.h"

using PhoneIds = std::string;  // one char per phone: index into GermanLexicon::phones()

struct LexiconPart {
    std::string spelling;  // lowercase UTF-8
    PhoneIds phones;
};

// German pronunciations in the MFA phone set: the German MFA dictionary, then compound splitting over it, then the
// g2p_de GRU (https://github.com/gooofy/g2p_de) for words it cannot cover.
class GermanLexicon {
public:
    // dict_path: german_mfa.dict; neural_model_path: g2p_de weights. A missing/unreadable file prints a warning to
    // stderr and that tier is skipped, as EnG2P does.
    GermanLexicon(const std::string& dict_path, const std::string& neural_model_path);

    // word: lowercase UTF-8 of a-z äöüß. Parts in order; empty = no pronunciation.
    std::vector<LexiconPart> lookup(const std::string& word) const;

    static const std::vector<std::string>& phones();  // g2p_de phoneme table incl. 4 specials
    static int phone_id(const std::string& name);     // index in phones(), -1 if absent

private:
    std::unordered_map<std::string, PhoneIds> dict_;
    NeuralG2P neural_;

    void load_dict(const std::string& path);
    PhoneIds predict(const std::string& word) const;
};
