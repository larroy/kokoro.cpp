// Unit checks for split_phonemes; needs neither the model nor ONNX Runtime.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "PhonemeChunker.h"

#include <algorithm>
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

}  // namespace

TEST_CASE("long_first_segment_has_no_empty_chunk") {
    const std::string segment(600, 'a');
    const std::vector<std::string> chunks = split_phonemes(segment + ". bc.", kLimit);
    CAPTURE(chunks.size());
    CHECK(chunks == std::vector<std::string>{segment, ". bc."});
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
    CHECK(std::all_of(chunks.begin() + 1, chunks.end(), [](const std::string& c) { return c.length() < kLimit; }));

    std::string joined;
    for (const std::string& c : chunks) joined += c;
    CHECK(strip_spaces(joined) == strip_spaces(input));
}

TEST_CASE("empty_input_yields_no_chunks") {
    CHECK(split_phonemes("", kLimit).empty());
}
