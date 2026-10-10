#include "GermanLexicon.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>

namespace {

using Dict = std::unordered_map<std::string, PhoneIds>;

constexpr size_t kMinPartLength = 3;  // code points per compound component
constexpr int kFirstRealPhone = 4;

// Symbol tables from g2p_de/g2p.py; indices must match the trained embeddings.
G2PSymbols german_symbols() {
    G2PSymbols symbols;
    symbols.graphemes = {"<pad>", "<unk>", "</s>"};
    for (char c = 'a'; c <= 'z'; ++c) symbols.graphemes.emplace_back(1, c);
    for (const char* c : {"ü", "ö", "ä", "ß"}) symbols.graphemes.emplace_back(c);
    symbols.phonemes = GermanLexicon::phones();
    return symbols;
}

// True when word is non-empty and every character is one of a-z äöüß.
bool is_german_word(const std::string& word) {
    if (word.empty()) return false;
    for (size_t i = 0; i < word.size(); ++i) {
        const auto c = static_cast<unsigned char>(word[i]);
        if (c >= 'a' && c <= 'z') continue;
        if (c != 0xC3 || i + 1 == word.size()) return false;
        const auto next = static_cast<unsigned char>(word[++i]);
        if (next != 0xA4 && next != 0xB6 && next != 0xBC && next != 0x9F) return false;  // ä ö ü ß
    }
    return true;
}

// Space-separated phone names to ids; false if any name is not a real phone of phones().
bool parse_phones(const std::string& field, PhoneIds& out) {
    size_t start = 0;
    while (start <= field.size()) {
        const size_t end = std::min(field.find(' ', start), field.size());
        const int id = GermanLexicon::phone_id(field.substr(start, end - start));
        if (id < kFirstRealPhone) return false;
        out += static_cast<char>(id);
        start = end + 1;
    }
    return true;
}

// Strips leading and trailing whitespace, as Python's str.strip() does for this ASCII-delimited file.
std::string trim(const std::string& line) {
    const char* ws = " \t\r\n";
    const size_t first = line.find_first_not_of(ws);
    if (first == std::string::npos) return "";
    return line.substr(first, line.find_last_not_of(ws) - first + 1);
}

// Byte offsets of every UTF-8 code point start in word, plus word.size().
std::vector<size_t> char_bounds(const std::string& word) {
    std::vector<size_t> bounds;
    for (size_t i = 0; i < word.size(); ++i) {
        if ((static_cast<unsigned char>(word[i]) & 0xC0) != 0x80) bounds.push_back(i);
    }
    bounds.push_back(word.size());
    return bounds;
}

// Splits a word into dictionary components of at least kMinPartLength code points, longest head first, allowing a
// linking s (Fugen-s) after a component. Failed start positions are memoized, bounding the work to O(n²) lookups.
class CompoundSplitter {
public:
    CompoundSplitter(const Dict& dict, const std::string& word)
        : dict_(dict), word_(word), bounds_(char_bounds(word)), failed_(bounds_.size(), 0) {}

    std::vector<LexiconPart> split() {
        std::vector<LexiconPart> parts;
        if (!split_from(0, parts)) parts.clear();
        return parts;
    }

private:
    const Dict& dict_;
    const std::string& word_;
    std::vector<size_t> bounds_;
    std::vector<char> failed_;

    size_t length() const { return bounds_.size() - 1; }

    std::string spelling(size_t start, size_t end) const {
        return word_.substr(bounds_[start], bounds_[end] - bounds_[start]);
    }

    // Appends the parts of word[start..] split into two or more components; false (parts unchanged) if impossible.
    bool split_from(size_t start, std::vector<LexiconPart>& parts) {
        if (failed_[start] || length() - start < 2 * kMinPartLength) return false;
        for (size_t end = length() - kMinPartLength; end >= start + kMinPartLength; --end) {
            std::optional<LexiconPart> head = head_part(start, end);
            if (!head) continue;
            const size_t mark = parts.size();
            parts.push_back(std::move(*head));
            if (resolve(end, parts)) return true;
            parts.resize(mark);
        }
        failed_[start] = 1;
        return false;
    }

    // Appends word[start..] as one dictionary entry or as a further split.
    bool resolve(size_t start, std::vector<LexiconPart>& parts) {
        const std::string tail = spelling(start, length());
        if (const auto it = dict_.find(tail); it != dict_.end()) {
            parts.push_back({tail, it->second});
            return true;
        }
        return split_from(start, parts);
    }

    // word[start, end) as a dictionary entry, or as an entry followed by a linking s.
    std::optional<LexiconPart> head_part(size_t start, size_t end) const {
        const std::string head = spelling(start, end);
        if (const auto it = dict_.find(head); it != dict_.end()) return LexiconPart{head, it->second};
        if (head.back() != 's' || end - 1 - start < kMinPartLength) return std::nullopt;
        const auto stem = dict_.find(head.substr(0, head.size() - 1));
        if (stem == dict_.end()) return std::nullopt;
        static const char s_id = static_cast<char>(GermanLexicon::phone_id("s"));
        return LexiconPart{head, stem->second + s_id};
    }
};

}  // namespace

const std::vector<std::string>& GermanLexicon::phones() {
    static const std::vector<std::string> table = {
        "<pad>", "<unk>", "<s>", "</s>",
        "ts", "ə", "iː", "oː", "pf", "aj", "d", "tʃ", "m", "œ", "z", "ɛ", "ɲ", "t", "ɟ", "n̩", "b", "ɪ", "kʰ", "h",
        "eː", "ɔ", "f", "v", "l̩", "n", "x", "yː", "p", "c", "aː", "ç", "uː", "ʃ", "øː", "a", "l", "j", "ɔʏ", "cʰ",
        "aw", "ŋ", "ɐ", "ʊ", "pʰ", "ʁ", "s", "ʏ", "ɡ", "tʰ", "k", "m̩"};
    return table;
}

int GermanLexicon::phone_id(const std::string& name) {
    static const std::unordered_map<std::string, int> ids = [] {
        std::unordered_map<std::string, int> map;
        for (size_t i = 0; i < phones().size(); ++i) map.emplace(phones()[i], static_cast<int>(i));
        return map;
    }();
    const auto it = ids.find(name);
    return it == ids.end() ? -1 : it->second;
}

GermanLexicon::GermanLexicon(const std::string& dict_path, const std::string& neural_model_path)
    : neural_(german_symbols()) {
    load_dict(dict_path);
    neural_.load(neural_model_path);
}

// word<TAB>p1 p2 …; among a word's variants the one with the most phones wins, the earlier line on a tie. MFA lists
// colloquial reductions first (fahren -> f aː n), so g2p_de's first-wins rule reads worse.
void GermanLexicon::load_dict(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[GermanLexicon] Warning: Failed to open dict: " << path << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        const std::string entry = trim(line);
        const size_t tab = entry.find('\t');
        if (tab == std::string::npos || entry.find('\t', tab + 1) != std::string::npos) continue;
        const std::string word = entry.substr(0, tab);
        PhoneIds phones;
        if (!is_german_word(word) || !parse_phones(entry.substr(tab + 1), phones)) continue;
        PhoneIds& slot = dict_[word];
        if (phones.size() > slot.size()) slot = std::move(phones);
    }
}

PhoneIds GermanLexicon::predict(const std::string& word) const {
    PhoneIds ids;
    for (const std::string& name : neural_.predict(word)) {
        const int id = phone_id(name);
        if (id >= kFirstRealPhone) ids += static_cast<char>(id);
    }
    return ids;
}

std::vector<LexiconPart> GermanLexicon::lookup(const std::string& word) const {
    if (const auto it = dict_.find(word); it != dict_.end()) return {{word, it->second}};
    if (std::vector<LexiconPart> parts = CompoundSplitter(dict_, word).split(); !parts.empty()) return parts;
    if (PhoneIds phones = predict(word); !phones.empty()) return {{word, std::move(phones)}};
    return {};
}
