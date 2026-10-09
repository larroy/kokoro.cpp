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
    {u"ranking", u"ránkin"}, {u"camping", u"cámpin"}, {u"jogging", u"yóguin"}, {u"hardware", u"járduer"},
    {u"online", u"onláin"}, {u"offline", u"ofláin"}, {u"smartphone", u"esmártfon"}, {u"email", u"ímeil"},
    {u"website", u"uébsait"}, {u"web", u"uéb"}, {u"wifi", u"uáifai"}, {u"facebook", u"féisbuk"},
    {u"youtube", u"yútub"}, {u"whatsapp", u"uátsap"}, {u"twitter", u"tuíter"}, {u"instagram", u"ínstagram"},
    {u"netflix", u"nétflix"}, {u"google", u"gúguel"}, {u"windows", u"uíndous"}, {u"iphone", u"áifon"},
    {u"ipad", u"áipad"}, {u"podcast", u"pódcast"}, {u"streaming", u"estrímin"}, {u"influencer", u"ínfluenser"},
    {u"newsletter", u"niúsleter"}, {u"bestseller", u"bestséler"}, {u"thriller", u"tríler"}, {u"blues", u"blus"},
    {u"rock", u"rok"}, {u"country", u"cántri"}, {u"playback", u"pléibak"}, {u"mouse", u"máus"},
    {u"blazer", u"bléiser"}, {u"spray", u"espréi"}, {u"scooter", u"escúter"}, {u"sandwich", u"sánduich"},
    {u"hobby", u"jóbi"}, {u"jersey", u"yérsei"}, {u"manager", u"mánayer"}, {u"iceberg", u"áisberg"},
    {u"spaghetti", u"espaguéti"}, {u"cappuccino", u"capuchíno"}, {u"gnocchi", u"ñóqui"}, {u"bruschetta", u"bruskéta"},
    {u"lasagna", u"lasáña"}, {u"mozzarella", u"motsaréla"}, {u"paparazzi", u"paparátsi"}, {u"graffiti", u"grafíti"},
    {u"tsunami", u"tsunámi"}, {u"kindergarten", u"kíndergarten"}, {u"kitsch", u"kich"}, {u"leitmotiv", u"láitmotif"},
    {u"pretzel", u"prétsel"}, {u"volkswagen", u"fólksvaguen"}, {u"sushi", u"súʃi"}, {u"sashimi", u"saʃími"},
    {u"geisha", u"guéiʃa"}, {u"anime", u"ánime"}, {u"judo", u"yúdo"}, {u"bazaar", u"basár"}, {u"yacht", u"yot"},
    {u"boutique", u"butík"}, {u"déja", u"déya"}, {u"faux", u"fó"}, {u"status", u"estátus"}, {u"quo", u"cuó"},
    {u"curriculum", u"currículum"}, {u"vitae", u"vítae"}, {u"résumé", u"resumé"}, {u"carte", u"cárt"},
};

// Lowercases text with spanish_to_lower after fold_grave.
std::u16string lowercase(std::u16string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](char16_t c) { return spanish_to_lower(fold_grave(c)); });
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

// Looks c up in the grave vowels and returns the plain vowel at the same index, keeping its case.
char16_t fold_grave(char16_t c) {
    static constexpr std::u16string_view grave = u"àèìòùÀÈÌÒÙ";
    static constexpr std::u16string_view plain = u"aeiouAEIOU";
    const size_t i = grave.find(c);
    return i == std::u16string_view::npos ? c : plain[i];
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
