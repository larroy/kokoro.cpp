#include "GermanG2P.h"

#include "GermanPronunciation.h"
#include "PhonemeText.h"
#include "Utils.h"

#include <algorithm>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

enum class SegmentType { Word, Punct, Space };

// One scanned piece of the utterance: a word, one punctuation mark, or a collapsed run of whitespace.
struct Segment {
    SegmentType type;
    std::u16string text;  // Word: lowercase letters a-z äöüß; Punct: the symbol; Space: " "
};

const std::u16string kKeptPunct = u";:,.!?—…\"()“”";

// Words espeak-ng reads differently from their stressed dictionary form.
const std::unordered_map<std::string, std::string> kOverrides = {
    {"der", "dɛɾ"}, {"er", "ɛɾ"}, {"des", "dɛs"}, {"nach", "nɑːx"}, {"es", "ɛs"}, {"zum", "ʦʊm"}};

// Function words espeak-ng leaves unstressed or only secondary-stressed.
const std::unordered_set<std::string> kUnstressed = {
    "der",   "die",    "das",    "den",    "dem",    "des",   "ein",   "eine",   "einen",  "einem",  "einer",
    "eines", "und",    "oder",   "aber",   "denn",   "doch",  "als",   "wie",    "wenn",   "dass",   "ob",
    "weil",  "in",     "im",     "an",     "am",     "auf",   "aus",   "bei",    "mit",    "nach",   "von",
    "vom",   "zu",     "zum",    "zur",    "für",    "über",  "unter", "vor",    "durch",  "gegen",  "ich",
    "du",    "er",     "sie",    "es",     "wir",    "ihr",   "mich",  "dich",   "sich",   "mir",    "dir",
    "uns",   "euch",   "ihn",    "ist",    "sind",   "bin",   "bist",  "war",    "hat",    "habe",   "hast",
    "haben", "wird",   "werden", "kann",   "soll",   "will",  "auch",  "schon",  "noch",   "da",     "dann",
    "mein",  "dein",   "sein",   "dies",   "wo",     "bis",   "kannst", "meine", "meinen", "meinem", "meiner",
    "meines", "deine", "deinen", "deinem", "deiner", "deines", "seine", "seinen", "seinem", "seiner", "seines"};

// ---- text normalization ----

// z. B. and d. h. spelled out.
std::string expand_abbreviations(const std::string& text) {
    static const std::regex zb(R"(\bz\.\s?b\.)", std::regex::ECMAScript | std::regex::icase);
    static const std::regex dh(R"(\bd\.\s?h\.)", std::regex::ECMAScript | std::regex::icase);
    return std::regex_replace(std::regex_replace(text, zb, "zum Beispiel"), dh, "das heißt");
}

// One German-formatted number in the form normalize_numbers reads: 1.000 -> 1000, 3,5 -> 3.5; others unchanged.
std::string plain_number(std::string number) {
    static const std::regex thousands(R"(\d{1,3}(?:\.\d{3})+)");
    static const std::regex decimal(R"(\d+,\d+)");
    if (std::regex_match(number, thousands)) {
        number.erase(std::remove(number.begin(), number.end(), '.'), number.end());
    } else if (std::regex_match(number, decimal)) {
        std::replace(number.begin(), number.end(), ',', '.');
    }
    return number;
}

// Rewrites German thousands separators and decimal commas, which belong to the text whatever the number language.
std::string convert_number_format(const std::string& text) {
    static const std::regex number(R"(\d+(?:[.,]\d+)*)");
    std::string out;
    auto last = text.cbegin();
    for (std::sregex_iterator it(text.begin(), text.end(), number), end; it != end; ++it) {
        out.append(last, (*it)[0].first);
        out += plain_number(it->str());
        last = (*it)[0].second;
    }
    out.append(last, text.cend());
    return out;
}

// ---- scanning ----

// ASCII, Latin-1 and ẞ capitals to lowercase.
char16_t german_to_lower(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c + 0x20);
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return static_cast<char16_t>(c + 0x20);
    return c == u'ẞ' ? u'ß' : c;
}

// Lowercase accented Latin letters foreign to German to their base letter; others unchanged.
char16_t fold_foreign(char16_t c) {
    static const std::u16string accented = u"àáâãåçèéêëìíîïñòóôõùúûýÿ";
    static const std::u16string plain = u"aaaaaceeeeiiiinoooouuuyy";
    const size_t i = accented.find(c);
    return i == std::u16string::npos ? c : plain[i];
}

bool is_letter(char16_t c) {
    return (c >= u'a' && c <= u'z') || std::u16string_view(u"äöüß").find(c) != std::u16string_view::npos;
}

// Kept punctuation with German and French quotes mapped to “ ”; 0 if c is not kept.
char16_t kept_punct(char16_t c) {
    if (c == u'„' || c == u'«') return u'“';
    if (c == u'»') return u'”';
    return kKeptPunct.find(c) == std::u16string::npos ? 0 : c;
}

bool is_separator(char16_t c) { return std::u16string_view(u"-– \t\n\r").find(c) != std::u16string_view::npos; }

// Extends the open Word, emits one Punct, or collapses a separator into Space. Apostrophes are skipped inside words
// (geht's); anything else is dropped.
void push_char(std::vector<Segment>& segments, char16_t input) {
    const char16_t c = fold_foreign(german_to_lower(input));
    if (is_letter(c)) {
        if (segments.empty() || segments.back().type != SegmentType::Word) segments.push_back({SegmentType::Word, u""});
        segments.back().text += c;
    } else if (const char16_t punct = kept_punct(c)) {
        segments.push_back({SegmentType::Punct, std::u16string(1, punct)});
    } else if (is_separator(c) && (segments.empty() || segments.back().type != SegmentType::Space)) {
        segments.push_back({SegmentType::Space, u" "});
    }
}

std::vector<Segment> scan(const std::u16string& text) {
    std::vector<Segment> segments;
    for (const char16_t c : text) {
        if (c != u'\'' && c != u'’') push_char(segments, c);
    }
    return segments;
}

// ---- rendering ----

// Kokoro phonemes for one lowercase word: an override, else its lexicon parts with ˈ on the first part unless the
// word is a single-part function word. Empty if the lexicon has no pronunciation.
std::string render_word(const std::string& word, const GermanLexicon& lexicon) {
    if (const auto it = kOverrides.find(word); it != kOverrides.end()) return it->second;
    const std::vector<LexiconPart> parts = lexicon.lookup(word);
    const bool unstressed = parts.size() == 1 && kUnstressed.count(word) > 0;
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += part_to_kokoro(parts[i], !unstressed && i == 0);
    return out;
}

std::u16string render_segment(const Segment& segment, const GermanLexicon& lexicon) {
    if (segment.type != SegmentType::Word) return segment.text;
    std::string word;
    BasicStringUtil::u16tou8(segment.text.c_str(), segment.text.size(), word);
    const std::string rendered = render_word(word, lexicon);
    std::u16string wide;
    BasicStringUtil::u8tou16(rendered.c_str(), rendered.size(), wide);
    return wide;
}

}  // namespace

std::string german_to_phonemes(const std::string& text, NumberLanguage numbers, const GermanLexicon& lexicon) {
    const std::string normalized = normalize_numbers(convert_number_format(expand_abbreviations(text)),
                                                     numbers == NumberLanguage::Auto ? NumberLanguage::German : numbers);
    std::u16string wide;
    BasicStringUtil::u8tou16(normalized.c_str(), normalized.size(), wide);
    std::u16string rendered;
    for (const Segment& segment : scan(wide)) rendered += render_segment(segment, lexicon);
    const std::u16string tidy = tidy_spaces(rendered);
    std::string result;
    BasicStringUtil::u16tou8(tidy.c_str(), tidy.size(), result);
    return result;
}
