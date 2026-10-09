#include "SpanishG2P.h"

#include "Utils.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace {

enum class Kind { Vowel, Nasal, Lateral, Rhotic, Other, Pause };

// One phonetic token: an IPA symbol (empty for a pause) plus the flags used for diphthongs, stress, and allophones.
struct Unit {
    std::u16string ipa;     // current symbol; later rewritten as a glide, offglide, or allophone
    Kind kind = Kind::Other;
    bool accented = false;  // written accent (áéíóú); that nucleus is stressed
    bool weak = false;      // i/u, or vowel y, that may join a neighboring vowel
    bool syllabic = false;  // weak vowel that never joins a diphthong (ü, u/i after r or ʎ)
    bool stressed = false;  // nucleus that receives the ˈ mark when the word is rendered
};

enum class SegmentType { Word, Punct, Space };

struct Segment {
    SegmentType type;
    std::u16string text;  // lowercased letters for Word, the symbol for Punct
    std::vector<Unit> units;
    bool all_caps = true;  // Word: every letter was written in upper case
};

const std::u16string kKeptPunct = u";:,.!?¿¡—…\"()“”";
const std::u16string kPausePunct = u",.;:!?¿¡—…()";
const std::u16string kNoSpaceBefore = u",.;:!?…)”";
const std::u16string kNoSpaceAfter = u"(“¿¡";
const std::array<std::u16string, 33> kUnstressed = {
    u"a",  u"al",  u"con", u"de",  u"del", u"e",  u"el",  u"en",  u"la", u"las", u"le", u"les",
    u"lo", u"los", u"me",  u"mi",  u"mis", u"ni", u"nos", u"o",   u"os", u"por", u"que", u"se",
    u"si", u"sin", u"su",  u"sus", u"te",  u"tu", u"tus", u"u",   u"y"};

bool contains(const std::u16string& set, char16_t c) { return set.find(c) != std::u16string::npos; }

char16_t to_lower(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c + 32);
    if (contains(u"ÁÉÍÓÚÜÑ", c)) return static_cast<char16_t>(c + 0x20);
    return c;
}

bool is_letter(char16_t c) { return (c >= u'a' && c <= u'z') || contains(u"áéíóúüñ", c); }

bool is_vowel_letter(char16_t c) { return contains(u"aeiouáéíóúü", c); }

bool is_front(char16_t c) { return contains(u"eiéí", c); }

// ---- scanning ----

void push_space(std::vector<Segment>& segments) {
    if (segments.empty() || segments.back().type != SegmentType::Space) segments.push_back({SegmentType::Space, u" "});
}

void push_char(std::vector<Segment>& segments, char16_t raw) {
    const char16_t c = to_lower(raw);
    if (is_letter(c)) {
        if (segments.empty() || segments.back().type != SegmentType::Word) segments.push_back({SegmentType::Word, u""});
        segments.back().text += c;
        segments.back().all_caps = segments.back().all_caps && raw != c;
    } else if (c == u'«' || c == u'»' || contains(kKeptPunct, c)) {
        const char16_t mapped = c == u'«' ? u'“' : c == u'»' ? u'”' : c;
        segments.push_back({SegmentType::Punct, std::u16string(1, mapped)});
    } else if (c == u'-' || c == u'–' || c == u' ' || c == u'\t' || c == u'\n' || c == u'\r') {
        push_space(segments);
    }
}

std::vector<Segment> scan(const std::u16string& text) {
    std::vector<Segment> segments;
    for (const char16_t c : text) push_char(segments, c);
    return segments;
}

// ---- letters to units ----

Unit vowel(char16_t c) {
    static const std::u16string accented = u"áéíóú";
    static const std::u16string plain = u"aeiou";
    const size_t acc = accented.find(c);
    if (acc != std::u16string::npos) return {std::u16string(1, plain[acc]), Kind::Vowel, true, false};
    if (c == u'ü') return {u"u", Kind::Vowel, false, true, true};
    return {std::u16string(1, c), Kind::Vowel, false, contains(u"iu", c)};
}

Unit consonant(const std::u16string& ipa) {
    if (contains(u"mnɲŋ", ipa[0])) return {ipa, Kind::Nasal};
    if (ipa == u"l" || ipa == u"ʎ") return {ipa, Kind::Lateral};
    if (ipa == u"ɾ" || ipa == u"r") return {ipa, Kind::Rhotic};
    return {ipa, Kind::Other};
}

// Units for the letters starting at w[i]; advances i past them.
Unit read_letter(const std::u16string& w, size_t& i) {
    const char16_t c = w[i];
    const char16_t next = i + 1 < w.size() ? w[i + 1] : u'\0';
    const char16_t after = i + 2 < w.size() ? w[i + 2] : u'\0';
    const size_t at = i++;
    if (is_vowel_letter(c)) return vowel(c);
    if ((c == u'c' && next == u'h') || (c == u'l' && next == u'l') || (c == u'r' && next == u'r')) {
        ++i;
        return consonant(c == u'c' ? u"ʧ" : c == u'l' ? u"ʎ" : u"r");
    }
    if ((c == u'q' || c == u'g') && next == u'u' && is_front(after)) {
        ++i;
        return consonant(c == u'q' ? u"k" : u"ɡ");
    }
    switch (c) {
    case u'r': return consonant(at == 0 || contains(u"lns", w[at - 1]) ? u"r" : u"ɾ");
    case u'q': return consonant(u"k");
    case u'g': return consonant(is_front(next) ? u"x" : u"ɡ");
    case u'c': return consonant(is_front(next) ? u"θ" : u"k");
    case u'z': return consonant(u"θ");
    case u'j': return consonant(u"x");
    case u'ñ': return consonant(u"ɲ");
    case u'x': return consonant(at == 0 ? u"s" : u"ks");
    case u'v': return consonant(u"b");
    case u'y': return is_vowel_letter(next) ? consonant(u"ʝ") : Unit{u"i", Kind::Vowel, false, true};
    default: return consonant(std::u16string(1, c));
    }
}

// Word-initial "ps" and "gn" drop their first letter: "psicología" -> sikoloxˈia, "gnomo" -> nˈomo.
size_t silent_prefix(const std::u16string& word) {
    const std::u16string start = word.substr(0, 2);
    return start == u"ps" || start == u"gn" ? 1 : 0;
}

std::vector<Unit> letters_to_units(const std::u16string& word) {
    std::vector<Unit> units;
    for (size_t i = silent_prefix(word); i < word.size();) {
        if (word[i] == u'h') {
            ++i;
            continue;
        }
        units.push_back(read_letter(word, i));
    }
    for (size_t i = 1; i < units.size(); ++i) {
        if (units[i].weak && !units[i].accented && (units[i - 1].ipa == u"r" || units[i - 1].ipa == u"ʎ")) {
            units[i].syllabic = true;
        }
    }
    return units;
}

// ---- nuclei and stress ----

struct Nucleus {
    size_t main;  // index of the main vowel in the word's units
    bool accented;
};

bool strong(const Unit& u) { return !u.weak || u.accented; }

// Main vowel of the vowel run [begin, end): the first strong one, else the last.
size_t main_vowel(const std::vector<Unit>& units, size_t begin, size_t end) {
    for (size_t i = begin; i < end; ++i) {
        if (strong(units[i])) return i;
    }
    return end - 1;
}

// Splits units into nuclei and turns weak vowels before a main vowel into glides.
std::vector<Nucleus> find_nuclei(std::vector<Unit>& units) {
    std::vector<Nucleus> nuclei;
    for (size_t begin = 0; begin < units.size();) {
        if (units[begin].kind != Kind::Vowel) {
            ++begin;
            continue;
        }
        size_t end = begin + 1;
        while (end < units.size() && units[end].kind == Kind::Vowel && !units[end - 1].syllabic &&
               !units[end].syllabic && !(strong(units[end]) && strong(units[end - 1]))) {
            ++end;
        }
        const size_t main = main_vowel(units, begin, end);
        for (size_t i = begin; i < main; ++i) units[i] = {units[i].ipa == u"i" ? u"j" : u"w", Kind::Vowel};
        for (size_t i = main + 1; i < end; ++i) units[i].ipa = units[i].ipa == u"i" ? u"ɪ" : u"ʊ";
        nuclei.push_back({main, units[main].accented});
        begin = end;
    }
    return nuclei;
}

bool unstressed_word(const std::u16string& word) {
    return std::find(kUnstressed.begin(), kUnstressed.end(), word) != kUnstressed.end();
}

// Index into nuclei of the stressed one, or nuclei.size() for none.
size_t stressed_nucleus(const std::vector<Nucleus>& nuclei, const std::u16string& word) {
    const auto accented = std::find_if(nuclei.begin(), nuclei.end(), [](const Nucleus& n) { return n.accented; });
    if (accented != nuclei.end()) return static_cast<size_t>(accented - nuclei.begin());
    if (nuclei.size() == 1) return unstressed_word(word) ? nuclei.size() : 0;
    if (nuclei.empty()) return 0;
    const char16_t last = word.back();
    const bool penult = is_vowel_letter(last) || last == u'n' || last == u's';
    return penult ? nuclei.size() - 2 : nuclei.size() - 1;
}

// espeak-ng opens a stressed e before n + consonant: "gente" -> xˈɛnte.
void open_stressed_e(std::vector<Unit>& units, size_t main) {
    const bool open = units[main].ipa == u"e" && main + 2 < units.size() && units[main + 1].ipa == u"n" &&
                      units[main + 2].kind != Kind::Vowel;
    if (open) units[main].ipa = u"ɛ";
}

void stress_nucleus(std::vector<Unit>& units, const std::vector<Nucleus>& nuclei, size_t stressed) {
    if (stressed >= nuclei.size()) return;
    units[nuclei[stressed].main].stressed = true;
    open_stressed_e(units, nuclei[stressed].main);
}

void phonemize_word(Segment& segment) {
    segment.units = letters_to_units(segment.text);
    const std::vector<Nucleus> nuclei = find_nuclei(segment.units);
    stress_nucleus(segment.units, nuclei, stressed_nucleus(nuclei, segment.text));
}

// ---- acronyms ----

const std::u16string kAlphabet = u"abcdefghijklmnñopqrstuvwxyzáéíóúü";
const std::array<std::u16string, 33> kLetterNames = {
    u"a",  u"be",   u"ce", u"de",  u"e",  u"efe", u"ge",  u"hache",    u"i",   u"jota",     u"ka",
    u"ele", u"eme", u"ene", u"eñe", u"o",  u"pe",  u"cu",  u"erre",     u"ese", u"te",       u"u",
    u"uve", u"uvedoble",   u"equis", u"igriega", u"zeta", u"a", u"e", u"i",  u"o", u"u", u"u"};
const std::array<std::u16string, 14> kOnsetClusters = {u"pl", u"pr", u"bl", u"br", u"tr", u"dr", u"cl",
                                                       u"cr", u"gl", u"gr", u"fl", u"fr", u"ch", u"ll"};

bool valid_onset(const std::u16string& s) {
    return s.size() <= 1 || std::find(kOnsetClusters.begin(), kOnsetClusters.end(), s) != kOnsetClusters.end();
}

// Word-initial onsets also include learned Greek clusters ("psicología", "gnomo").
bool valid_initial_onset(const std::u16string& s) { return valid_onset(s) || s == u"ps" || s == u"gn"; }

// Consonants that close a syllable inside a word ("ap-to", "ins-tar", "pers-pec-ti-va").
bool valid_medial_coda(const std::u16string& s) {
    return s.empty() || (s.size() == 1 && !contains(u"hñqvw", s[0])) || s == u"ns" || s == u"bs" || s == u"rs" ||
           s == u"ls";
}

// A word can end in one consonant: native endings ("otan", "reloj") and loanwords ("álbum", "robot", "club").
bool valid_final_coda(const std::u16string& s) { return s.size() <= 1; }

bool valid_medial(const std::u16string& s) {
    for (size_t split = 0; split <= std::min<size_t>(2, s.size() - 1); ++split) {
        if (valid_medial_coda(s.substr(0, split)) && valid_onset(s.substr(split))) return true;
    }
    return false;
}

// True when every consonant run fits Spanish syllables: onset, medial coda + onset, final coda.
bool pronounceable(const std::u16string& w) {
    const auto first_vowel = std::find_if(w.begin(), w.end(), is_vowel_letter);
    if (first_vowel == w.end() || !valid_initial_onset(std::u16string(w.begin(), first_vowel))) return false;
    std::u16string run;
    for (auto it = first_vowel; it != w.end(); ++it) {
        if (!is_vowel_letter(*it)) {
            run += *it;
        } else if (!run.empty()) {
            if (!valid_medial(run)) return false;
            run.clear();
        }
    }
    return valid_final_coda(run);
}

// An all-caps word is read as a word when Spanish can say it as written ("ONU", "OTAN"), and spelled out by
// letter names otherwise, since that is shorter and easier than the letters ("CPP", "GPU", "DVD", "IBM").
bool spelled_acronym(const Segment& segment) {
    return segment.all_caps && segment.text.size() >= 2 && !pronounceable(segment.text);
}

// Letter names read as one word with the main stress on the last letter: "CPP" -> θepepˈe.
void spell_word(Segment& segment) {
    std::vector<Nucleus> last_nuclei;
    std::u16string last_name;
    size_t offset = 0;
    for (const char16_t letter : segment.text) {
        last_name = kLetterNames[kAlphabet.find(letter)];
        std::vector<Unit> units = letters_to_units(last_name);
        last_nuclei = find_nuclei(units);
        offset = segment.units.size();
        segment.units.insert(segment.units.end(), units.begin(), units.end());
    }
    // A one-syllable name is stressed even when it is also an unstressed word ("de", "te").
    const size_t stressed = last_nuclei.size() == 1 ? 0 : stressed_nucleus(last_nuclei, last_name);
    for (Nucleus& nucleus : last_nuclei) nucleus.main += offset;
    stress_nucleus(segment.units, last_nuclei, stressed);
}

// ---- allophones ----

const Unit kPause{u"", Kind::Pause};

// Units of the utterance in order, with kPause at the edges and at pause punctuation.
std::vector<Unit*> flatten(std::vector<Segment>& segments) {
    std::vector<Unit*> flat{const_cast<Unit*>(&kPause)};
    for (Segment& segment : segments) {
        if (segment.type == SegmentType::Punct && contains(kPausePunct, segment.text[0])) {
            flat.push_back(const_cast<Unit*>(&kPause));
        }
        for (Unit& unit : segment.units) flat.push_back(&unit);
    }
    flat.push_back(const_cast<Unit*>(&kPause));
    return flat;
}

bool kind_in(const Unit& u, std::initializer_list<Kind> kinds) {
    return std::find(kinds.begin(), kinds.end(), u.kind) != kinds.end();
}

// espeak-ng phsource/ph_spanish: n assimilates to the next consonant's place; voiced stops become approximants
// after continuants.
std::u16string allophone(const Unit& prev, const Unit& unit, const Unit& next) {
    if (unit.ipa == u"n" && (next.ipa == u"p" || next.ipa == u"b")) return u"m";
    if (unit.ipa == u"n" && (next.ipa == u"ɡ" || next.ipa == u"x")) return u"ŋ";
    const bool after_stop = kind_in(prev, {Kind::Pause, Kind::Nasal});
    if (unit.ipa == u"b" && !after_stop && kind_in(next, {Kind::Vowel, Kind::Rhotic, Kind::Lateral})) return u"β";
    if (unit.ipa == u"ɡ" && !after_stop && kind_in(next, {Kind::Vowel, Kind::Rhotic, Kind::Lateral})) return u"ɣ";
    if (unit.ipa == u"d" && !after_stop && kind_in(next, {Kind::Vowel, Kind::Rhotic})) return u"ð";
    return unit.ipa;
}

void apply_allophones(std::vector<Segment>& segments) {
    const std::vector<Unit*> flat = flatten(segments);
    std::vector<std::u16string> replaced(flat.size());
    for (size_t i = 1; i + 1 < flat.size(); ++i) replaced[i] = allophone(*flat[i - 1], *flat[i], *flat[i + 1]);
    for (size_t i = 1; i + 1 < flat.size(); ++i) {
        if (flat[i] != &kPause) flat[i]->ipa = replaced[i];
    }
}

// ---- rendering ----

// Kokoro's e2m symbols for espeak-ng's falling diphthongs.
const std::array<std::pair<std::u16string, std::u16string>, 4> kDiphthongs = {
    {{u"aɪ", u"I"}, {u"aʊ", u"W"}, {u"eɪ", u"A"}, {u"oʊ", u"O"}}};

std::u16string merge_diphthongs(std::u16string text) {
    for (const auto& [from, to] : kDiphthongs) {
        for (size_t pos = text.find(from); pos != std::u16string::npos; pos = text.find(from, pos)) {
            text.replace(pos, from.size(), to);
        }
    }
    return text;
}

std::u16string render_segment(const Segment& segment) {
    if (segment.type != SegmentType::Word) return segment.text;
    std::u16string out;
    for (const Unit& unit : segment.units) out += (unit.stressed ? u"ˈ" : u"") + unit.ipa;
    return merge_diphthongs(out);
}

// Drops spaces before closing / after opening punctuation, collapses runs and trims.
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

}  // namespace

std::string spanish_to_phonemes(const std::string& text, NumberLanguage numbers) {
    const std::string normalized =
        normalize_numbers(text, numbers == NumberLanguage::Auto ? NumberLanguage::Spanish : numbers);
    std::u16string wide;
    BasicStringUtil::u8tou16(normalized.c_str(), normalized.size(), wide);
    std::vector<Segment> segments = scan(wide);
    for (Segment& segment : segments) {
        if (segment.type != SegmentType::Word) continue;
        if (spelled_acronym(segment)) {
            spell_word(segment);
        } else {
            phonemize_word(segment);
        }
    }
    apply_allophones(segments);
    std::u16string rendered;
    for (const Segment& segment : segments) rendered += render_segment(segment);
    const std::u16string tidy = tidy_spaces(rendered);
    std::string result;
    BasicStringUtil::u16tou8(tidy.c_str(), tidy.size(), result);
    return result;
}
