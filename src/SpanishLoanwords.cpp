#include "SpanishLoanwords.h"

#include "Utils.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

// Common loanwords and their usual Spanish pronunciation, spelled as Spanish.
const std::unordered_map<std::u16string, std::u16string> kBuiltin = {
    {u"parking", u"párkin"},  {u"marketing", u"márketin"}, {u"show", u"ʃóu"},       {u"shampoo", u"ʃampú"},
    {u"shopping", u"ʃópin"},  {u"software", u"sóftuer"},   {u"whisky", u"uíski"},   {u"whiskey", u"uíski"},
    {u"güisqui", u"gwíski"},  {u"croissant", u"cruasán"},  {u"pizza", u"pítsa"},    {u"jazz", u"yas"},
    {u"facebook", u"féisbuk"},
};

// Lowercases text with spanish_to_lower.
std::u16string lowercase(std::u16string text) {
    std::transform(text.begin(), text.end(), text.begin(), spanish_to_lower);
    return text;
}

// UTF-8 to UTF-16, lowercased.
std::u16string lower_utf16(const std::string& text) {
    std::u16string wide;
    BasicStringUtil::u8tou16(text.c_str(), text.size(), wide);
    return lowercase(std::move(wide));
}

// The word and respelling of a TSV line, or nullopt for a comment, blank, or malformed line.
std::optional<std::pair<std::u16string, std::u16string>> parse_line(const std::string& line) {
    if (line.empty() || line[0] == '#') return std::nullopt;
    const size_t tab = line.find('\t');
    if (tab == std::string::npos || tab == 0 || tab + 1 == line.size()) {
        std::cerr << "[SpanishLoanwords] Warning: Skipping malformed line: " << line << std::endl;
        return std::nullopt;
    }
    return std::make_pair(lower_utf16(line.substr(0, tab)), lower_utf16(line.substr(tab + 1)));
}

}  // namespace

char16_t spanish_to_lower(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c + 32);
    if (std::u16string_view(u"ÁÉÍÓÚÜÑ").find(c) != std::u16string_view::npos) return static_cast<char16_t>(c + 0x20);
    return c;
}

const SpanishLoanwords& SpanishLoanwords::builtin() {
    static const SpanishLoanwords table = [] {
        SpanishLoanwords loanwords;
        loanwords.entries_ = kBuiltin;
        return loanwords;
    }();
    return table;
}

SpanishLoanwords SpanishLoanwords::load(const std::string& tsv_path) {
    SpanishLoanwords loanwords = builtin();
    std::ifstream file(tsv_path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (auto entry = parse_line(line)) loanwords.entries_[entry->first] = std::move(entry->second);
    }
    return loanwords;
}

std::optional<std::u16string> SpanishLoanwords::respell(const std::u16string& word) const {
    if (const auto it = entries_.find(word); it != entries_.end()) return it->second;
    if (word.size() < 2 || word.back() != u's') return std::nullopt;
    const auto stem = entries_.find(word.substr(0, word.size() - 1));
    return stem == entries_.end() ? std::nullopt : std::optional(stem->second + u"s");
}
