#include "GermanPronunciation.h"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// How one MFA phone renders in Kokoro's espeak-ng `de` symbols. Every symbol must exist in dict/vocab.txt:
// PhonemeEncoder::encode silently drops unknown symbols.
struct PhoneInfo {
    std::string kokoro;    // context-free symbol
    std::string devoiced;  // symbol at the end of a part (final devoicing); empty: same as kokoro
    bool stressable = false;
    bool vowel = false;
    bool obstruent = false;
};

// MFA phones whose Kokoro symbol differs from their MFA name.
const std::unordered_map<std::string, std::string> kMapped = {
    {"ts", "ʦ"}, {"aj", "I"},  {"aw", "W"},  {"ɔʏ", "ɔø"}, {"aː", "ɑː"}, {"ʏ", "y"},   {"pʰ", "p"},  {"tʰ", "t"},
    {"kʰ", "k"}, {"c", "k"},   {"cʰ", "k"},  {"ɟ", "ɡ"},   {"ɲ", "n"},   {"n̩", "ən"}, {"l̩", "əl"}, {"m̩", "əm"}};
const std::unordered_map<std::string, std::string> kDevoiced = {{"b", "p"}, {"d", "t"}, {"ɡ", "k"},
                                                                {"ɟ", "k"}, {"z", "s"}, {"v", "f"}};
const std::unordered_set<std::string> kStressable = {"iː", "oː", "aj", "œ", "ɛ",  "ɪ",  "eː", "ɔ", "yː",
                                                     "aː", "uː", "øː", "a", "ɔʏ", "aw", "ʊ",  "ʏ"};
const std::unordered_set<std::string> kReducedVowels = {"ə", "ɐ", "n̩", "l̩", "m̩"};
// f is left out: espeak-ng writes fr, not fɾ.
const std::unordered_set<std::string> kObstruents = {"b", "p", "pʰ", "d", "t", "tʰ", "ɡ", "ɟ", "k", "kʰ", "c", "cʰ"};

// Stress position counted in stressable vowels from the end (1 = last) for spellings ending in a suffix; the first
// matching entry wins.
const std::array<std::pair<const char*, size_t>, 21> kStressedSuffixes = {{
    {"ierung", 2}, {"itäten", 1}, {"ieren", 1}, {"ismus", 2}, {"ionen", 1}, {"enten", 1}, {"anten", 1},
    {"isten", 1},  {"ität", 1},   {"iert", 1},  {"ion", 1},   {"eur", 1},   {"ell", 1},   {"ant", 1},
    {"ent", 1},    {"anz", 1},    {"enz", 1},   {"ist", 1},   {"ei", 1},    {"ur", 1},    {"iv", 1},
}};
// Unstressed prefixes whose ɛ is skipped by stress.
const std::array<const char*, 5> kUnstressedPrefixes = {"ver", "zer", "ent", "emp", "er"};

PhoneInfo make_info(const std::string& name) {
    const auto mapped = kMapped.find(name);
    const auto devoiced = kDevoiced.find(name);
    PhoneInfo info;
    info.kokoro = mapped == kMapped.end() ? name : mapped->second;
    info.devoiced = devoiced == kDevoiced.end() ? "" : devoiced->second;
    info.stressable = kStressable.count(name) > 0;
    info.vowel = info.stressable || kReducedVowels.count(name) > 0;
    info.obstruent = kObstruents.count(name) > 0;
    return info;
}

// PhoneInfo per phone id of GermanLexicon::phones(); specials render as nothing.
const std::vector<PhoneInfo>& phone_infos() {
    static const std::vector<PhoneInfo> table = [] {
        std::vector<PhoneInfo> infos(GermanLexicon::phones().size());
        for (size_t i = 4; i < infos.size(); ++i) infos[i] = make_info(GermanLexicon::phones()[i]);
        return infos;
    }();
    return table;
}

const PhoneInfo& info_of(char id) { return phone_infos()[static_cast<size_t>(id)]; }

int id_of(const char* name) { return GermanLexicon::phone_id(name); }

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

// Vocalic r (MFA ɐ): after a full vowel it is the r of a diphthong, otherwise espeak-ng's unstressed ɜ ending.
std::string_view vocalic_r(int prev) {
    static const int u = id_of("ʊ");
    if (prev >= 0 && info_of(static_cast<char>(prev)).stressable) return prev == u ? "ɐ" : "ɾ";
    return "ɜ";
}

// Consonantal r (MFA ʁ): vocalized before a consonant or the end, a tap after a stop, a trill before a vowel.
std::string_view consonant_r(int prev, int next) {
    static const int u = id_of("ʊ");
    if (next < 0 || !info_of(static_cast<char>(next)).vowel) return prev == u ? "ɐ" : "ɾ";
    if (prev >= 0 && info_of(static_cast<char>(prev)).obstruent) return "ɾ";
    return "r";
}

// Kokoro symbol for phones[i] in the context of its neighbours within the part.
std::string_view symbol_at(const PhoneIds& phones, size_t i) {
    static const int vocalic = id_of("ɐ"), consonantal = id_of("ʁ");
    const int id = phones[i];
    const int prev = i > 0 ? phones[i - 1] : -1;
    const int next = i + 1 < phones.size() ? phones[i + 1] : -1;
    if (id == vocalic) return vocalic_r(prev);
    if (id == consonantal) return consonant_r(prev, next);
    const PhoneInfo& info = info_of(phones[i]);
    return next < 0 && !info.devoiced.empty() ? std::string_view(info.devoiced) : std::string_view(info.kokoro);
}

std::vector<size_t> stressable_positions(const PhoneIds& phones) {
    std::vector<size_t> positions;
    for (size_t i = 0; i < phones.size(); ++i) {
        if (info_of(phones[i]).stressable) positions.push_back(i);
    }
    return positions;
}

// Stressed vowel among positions (non-empty): suffix rules, then -ie, then unstressed prefixes, else the first.
size_t stress_position(const LexiconPart& part, const std::vector<size_t>& positions) {
    static const char long_i = static_cast<char>(id_of("iː")), open_e = static_cast<char>(id_of("ɛ"));
    if (positions.size() < 2) return positions.front();
    const auto suffix = std::find_if(kStressedSuffixes.begin(), kStressedSuffixes.end(),
                                     [&](const auto& entry) { return ends_with(part.spelling, entry.first); });
    if (suffix != kStressedSuffixes.end()) return positions[positions.size() - std::min(suffix->second, positions.size())];
    if (ends_with(part.spelling, "ie") && part.phones.back() == long_i) return positions.back();
    const bool prefixed = std::any_of(kUnstressedPrefixes.begin(), kUnstressedPrefixes.end(),
                                      [&](const char* prefix) { return starts_with(part.spelling, prefix); });
    if (prefixed && part.phones[positions.front()] == open_e) return positions[1];
    return positions.front();
}

}  // namespace

std::string part_to_kokoro(const LexiconPart& part, bool stressed) {
    const std::vector<size_t> positions = stressable_positions(part.phones);
    const size_t stress = stressed && !positions.empty() ? stress_position(part, positions) : part.phones.size();
    std::string out;
    for (size_t i = 0; i < part.phones.size(); ++i) {
        if (i == stress) out += "ˈ";
        out += symbol_at(part.phones, i);
    }
    return out;
}
