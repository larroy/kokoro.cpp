#include "PhonemeChunker.h"

#include "Utf8.h"

#include <algorithm>
#include <array>
#include <regex>
#include <string_view>

namespace {

const std::string kPunctuation = ".,!?;";
const std::array<std::string_view, 4> kToneMarks = {"→", "↗", "↘", "↓"};

// Regex parts of phonemes split at [.,!?;] (marks kept as their own parts), trimmed, empties dropped.
std::vector<std::string> segments(const std::string& phonemes) {
    static const std::regex split_re("([.,!?;])");
    static const std::regex trim_re("^\\s+|\\s+$");
    std::vector<std::string> parts;
    for (std::sregex_token_iterator it(phonemes.begin(), phonemes.end(), split_re, {-1, 0}), end; it != end; ++it) {
        std::string part = std::regex_replace(it->str(), trim_re, "");
        if (!part.empty()) parts.push_back(std::move(part));
    }
    return parts;
}

bool is_punctuation(const std::string& segment) {
    return segment.size() == 1 && kPunctuation.find(segment[0]) != std::string::npos;
}

// Each punctuation segment joins the group before it; a leading one opens its own group.
std::vector<std::string> attach_punctuation(const std::vector<std::string>& segments) {
    std::vector<std::string> groups;
    for (const std::string& segment : segments) {
        if (is_punctuation(segment) && !groups.empty()) groups.back() += segment;
        else groups.push_back(segment);
    }
    return groups;
}

bool is_tone_mark(const std::string& word, std::size_t pos, std::size_t next) {
    const std::string_view character(word.data() + pos, next - pos);
    return std::find(kToneMarks.begin(), kToneMarks.end(), character) != kToneMarks.end();
}

// Byte end of the piece of word starting at begin: the rest if it fits in max_length characters, else just past the
// last tone mark within the window, else exactly max_length characters. A cut that would leave only the trailing
// punctuation (which starts after last_core_start) moves back to last_core_start.
std::size_t hard_cut(const std::string& word, std::size_t begin, std::size_t max_length, std::size_t last_core_start) {
    std::size_t end = begin;
    std::size_t after_tone = begin;
    for (std::size_t n = 0; n < max_length && end < word.size(); ++n) {
        const std::size_t next = utf8_next(word, end);
        if (is_tone_mark(word, end, next)) after_tone = next;
        end = next;
    }
    if (end == word.size()) return end;
    if (after_tone > begin) end = after_tone;
    if (end > last_core_start && last_core_start > begin) end = last_core_start;
    return end;
}

// Byte offset where the last non-punctuation character of word starts; npos if word is all punctuation.
std::size_t last_core_char_start(const std::string& word) {
    const std::size_t core_end = word.find_last_not_of(kPunctuation);
    if (core_end == std::string::npos) return std::string::npos;
    std::size_t start = core_end;
    while (start > 0 && (static_cast<unsigned char>(word[start]) & 0xC0) == 0x80) --start;
    return start;
}

// word cut into pieces of at most max_length characters by hard_cut.
std::vector<std::string> hard_split(const std::string& word, std::size_t max_length) {
    const std::size_t core_start = last_core_char_start(word);
    std::vector<std::string> pieces;
    for (std::size_t begin = 0; begin < word.size();) {
        const std::size_t end = hard_cut(word, begin, max_length, core_start);
        pieces.push_back(word.substr(begin, end - begin));
        begin = end;
    }
    return pieces;
}

// group if it fits; else its space-separated words (empty ones dropped), each word over the limit hard-split.
std::vector<std::string> pieces(const std::string& group, std::size_t max_length) {
    if (utf8_length(group) <= max_length) return {group};
    std::vector<std::string> out;
    std::size_t begin = 0;
    while (begin <= group.size()) {
        const std::size_t space = std::min(group.find(' ', begin), group.size());
        const std::string word = group.substr(begin, space - begin);
        if (utf8_length(word) > max_length) {
            const std::vector<std::string> parts = hard_split(word, max_length);
            out.insert(out.end(), parts.begin(), parts.end());
        } else if (!word.empty()) {
            out.push_back(word);
        }
        begin = space + 1;
    }
    return out;
}

// Joins pieces with single spaces, starting a new chunk when the next piece would push it past max_length.
std::vector<std::string> pack(const std::vector<std::string>& pieces, std::size_t max_length) {
    std::vector<std::string> chunks;
    std::string current;
    std::size_t current_length = 0;
    for (const std::string& piece : pieces) {
        const std::size_t piece_length = utf8_length(piece);
        if (!current.empty() && current_length + 1 + piece_length <= max_length) {
            current += ' ' + piece;
            current_length += 1 + piece_length;
            continue;
        }
        if (!current.empty()) chunks.push_back(std::move(current));
        current = piece;
        current_length = piece_length;
    }
    if (!current.empty()) chunks.push_back(std::move(current));
    return chunks;
}

}  // namespace

std::vector<std::string> split_phonemes(const std::string& phonemes, std::size_t max_length) {
    std::vector<std::string> all_pieces;
    for (const std::string& group : attach_punctuation(segments(phonemes))) {
        const std::vector<std::string> group_pieces = pieces(group, max_length);
        all_pieces.insert(all_pieces.end(), group_pieces.begin(), group_pieces.end());
    }
    return pack(all_pieces, max_length);
}
