// Unit checks for split_phonemes; needs neither the model nor ONNX Runtime.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "PhonemeChunker.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kLimit = 510;

std::string strip_spaces(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

std::string repeat(const std::string& unit, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) out += unit;
    return out;
}

bool is_continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

std::size_t code_points(const std::string& s) {
    return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), [](char c) { return !is_continuation(c); }));
}

// Every lead byte announces 1-4 bytes and that many continuation bytes follow inside the string.
bool is_valid_utf8(const std::string& s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const std::size_t len =
            c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (len == 0 || i + len > s.size()) return false;
        if (!std::all_of(s.begin() + i + 1, s.begin() + i + len, is_continuation)) return false;
        i += len;
    }
    return true;
}

std::string join(const std::vector<std::string>& chunks) {
    std::string out;
    for (const std::string& c : chunks) out += c;
    return out;
}

bool starts_with_punctuation(const std::string& s) {
    return !s.empty() && std::string(".,!?;").find(s[0]) != std::string::npos;
}

std::vector<std::size_t> sizes_in_code_points(const std::vector<std::string>& chunks) {
    std::vector<std::size_t> sizes;
    std::transform(chunks.begin(), chunks.end(), std::back_inserter(sizes), code_points);
    return sizes;
}

bool all_bounded(const std::vector<std::string>& chunks) {
    return std::all_of(chunks.begin(), chunks.end(), [](const std::string& c) { return code_points(c) <= kLimit; });
}

}  // namespace

TEST_CASE("long_first_segment_has_no_empty_chunk") {
    const std::string segment(600, 'a');
    const std::vector<std::string> chunks = split_phonemes(segment + ". bc.", kLimit);
    CAPTURE(chunks.size());
    CHECK(chunks == std::vector<std::string>{std::string(510, 'a'), std::string(90, 'a') + ". bc."});
}

TEST_CASE("first_segment_at_the_limit_boundary") {
    for (const std::size_t n : {508, 509, 510}) {
        CAPTURE(n);
        const std::string segment(n, 'a');
        CHECK(split_phonemes(segment, kLimit) == std::vector<std::string>{segment});
    }
}

TEST_CASE("chunks_are_nonempty_bounded_and_lossless") {
    const std::string input = std::string(600, 'a') + ". " + repeat("ni xau, ", 200);
    const std::vector<std::string> chunks = split_phonemes(input, kLimit);
    CAPTURE(chunks.size());
    REQUIRE(chunks.size() > 2);

    CHECK(std::none_of(chunks.begin(), chunks.end(), [](const std::string& c) { return c.empty(); }));
    CHECK(all_bounded(chunks));
    CHECK(strip_spaces(join(chunks)) == strip_spaces(input));
}

TEST_CASE("empty_input_yields_no_chunks") {
    CHECK(split_phonemes("", kLimit).empty());
}

TEST_CASE("boundary_units_around_the_limit") {
    for (const std::string unit : {"a", "ʂ"}) {
        CAPTURE(unit);
        CHECK(split_phonemes(repeat(unit, 509), kLimit) == std::vector<std::string>{repeat(unit, 509)});
        CHECK(split_phonemes(repeat(unit, 510), kLimit) == std::vector<std::string>{repeat(unit, 510)});
        CHECK(split_phonemes(repeat(unit, 511), kLimit) == std::vector<std::string>{repeat(unit, 510), unit});
    }
    const std::string two_words = repeat("a", 254) + " " + repeat("a", 255);
    CHECK(split_phonemes(two_words, kLimit) == std::vector<std::string>{two_words});
    CHECK(split_phonemes(repeat("a", 254) + " " + repeat("a", 256), kLimit) ==
          std::vector<std::string>{repeat("a", 254), repeat("a", 256)});
    CHECK(split_phonemes(repeat("a", 509) + ".", kLimit) == std::vector<std::string>{repeat("a", 509) + "."});
    CHECK(split_phonemes(repeat("a", 510) + ".", kLimit) == std::vector<std::string>{repeat("a", 509), "a."});
}

TEST_CASE("long_ascii_stretch_splits_at_word_boundaries") {
    const std::string input = repeat("hello world ", 100);
    const std::vector<std::string> chunks = split_phonemes(input, kLimit);
    CAPTURE(sizes_in_code_points(chunks));
    REQUIRE(chunks.size() > 1);
    CHECK(all_bounded(chunks));
    for (const std::string& chunk : chunks) {
        std::istringstream words(chunk);
        for (std::string word; words >> word;) CHECK((word == "hello" || word == "world"));
    }
    CHECK(std::all_of(chunks.begin(), chunks.end() - 1,
                      [](const std::string& c) { return code_points(c) > kLimit - 6; }));
    CHECK(strip_spaces(join(chunks)) == strip_spaces(input));
}

TEST_CASE("long_chinese_stretch_is_cut_after_a_tone_mark") {
    // 16 characters, 28 bytes per unit.
    const std::string input = repeat("ʈʂʊ→ŋkwo↗ni↘xau↓", 60);
    const std::vector<std::string> chunks = split_phonemes(input, kLimit);
    CAPTURE(sizes_in_code_points(chunks));
    REQUIRE(chunks.size() == 2);
    // The 510-character window holds 31 units plus ʈʂʊ→ŋkwo↗ni↘xa; its last tone mark ↘ is character 508.
    CHECK(code_points(chunks[0]) == 508);
    const std::string last_tone = "↘";
    CHECK(chunks[0].compare(chunks[0].size() - last_tone.size(), last_tone.size(), last_tone) == 0);
    CHECK(chunks[0].size() > kLimit);
    CHECK(std::all_of(chunks.begin(), chunks.end(), is_valid_utf8));
    CHECK(join(chunks) == input);
}

TEST_CASE("space_free_word_over_the_limit_is_hard_split") {
    const std::string input = repeat("ʈʂɕ", 400);
    const std::vector<std::string> chunks = split_phonemes(input, kLimit);
    CHECK(sizes_in_code_points(chunks) == std::vector<std::size_t>{510, 510, 180});
    CHECK(std::all_of(chunks.begin(), chunks.end(), is_valid_utf8));
    CHECK(join(chunks) == input);
}

TEST_CASE("mixed_punctuation_and_long_stretches") {
    const std::string input = "ni xau, " + repeat("wo ai ni ", 120) + "xau! " + repeat("ʈʂʊ→ŋkwo↗", 70) + ".";
    const std::vector<std::string> chunks = split_phonemes(input, kLimit);
    CAPTURE(sizes_in_code_points(chunks));
    REQUIRE(chunks.size() > 2);
    CHECK(std::none_of(chunks.begin(), chunks.end(), [](const std::string& c) { return c.empty(); }));
    CHECK(all_bounded(chunks));
    CHECK(std::all_of(chunks.begin(), chunks.end(), is_valid_utf8));
    CHECK(std::none_of(chunks.begin(), chunks.end(), starts_with_punctuation));
    CHECK(strip_spaces(join(chunks)) == strip_spaces(input));
}

TEST_CASE("trailing_punctuation_keeps_the_final_toned_syllable") {
    for (const std::string tone : {"→", "↗", "↘", "↓"}) {
        for (const std::string punctuation : {".", "...", "!?"}) {
            CAPTURE(tone);
            CAPTURE(punctuation);
            const std::string syllable = "ni" + tone;
            const std::string input = repeat(syllable, 170) + punctuation;
            const auto chunks = split_phonemes(input, kLimit);
            CHECK(chunks == std::vector<std::string>{repeat(syllable, 169), syllable + punctuation});
            CHECK(all_bounded(chunks));
            CHECK(join(chunks) == input);
            CHECK(std::all_of(chunks.begin(), chunks.end(), is_valid_utf8));
        }
    }
}

TEST_CASE("trailing_tone_without_an_earlier_tone_keeps_its_phoneme") {
    const std::string input = repeat("ʂ", 509) + "→.";
    const auto chunks = split_phonemes(input, kLimit);
    CHECK(chunks == std::vector<std::string>{repeat("ʂ", 508), "ʂ→."});
    CHECK(join(chunks) == input);
    CHECK(std::all_of(chunks.begin(), chunks.end(), is_valid_utf8));
}

TEST_CASE("punctuation_boundary_precedes_splitting_an_oversized_group") {
    const std::string first_sentence = std::string(100, 'a') + ".";
    const std::string word(199, 'b');
    const std::string input = first_sentence + " " + word + " " + word + " " + word + ". c.";
    const auto chunks = split_phonemes(input, kLimit);
    CHECK(chunks == std::vector<std::string>{first_sentence, word + " " + word, word + ". c."});
    CHECK(strip_spaces(join(chunks)) == strip_spaces(input));
    CHECK(all_bounded(chunks));
}

TEST_CASE("punctuation_boundary_precedes_hard_splitting_a_word") {
    const std::string first_sentence = std::string(100, 'a') + ".";
    const std::string syllable = repeat("ʂ", 398) + "→";
    const std::string input = first_sentence + " " + repeat(syllable, 2) + ".";
    const auto chunks = split_phonemes(input, kLimit);
    CHECK(chunks == std::vector<std::string>{first_sentence, syllable, syllable + "."});
    CHECK(strip_spaces(join(chunks)) == strip_spaces(input));
    CHECK(all_bounded(chunks));
}
