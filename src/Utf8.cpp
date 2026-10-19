#include "Utf8.h"

#include <algorithm>

std::size_t utf8_next(const std::string& s, std::size_t pos) {
    const auto c = static_cast<unsigned char>(s[pos]);
    std::size_t len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    return std::min(pos + len, s.size());
}

std::size_t utf8_length(const std::string& s) {
    std::size_t count = 0;
    for (std::size_t pos = 0; pos < s.size(); pos = utf8_next(s, pos)) ++count;
    return count;
}

std::vector<std::string> split_utf8(const std::string& s) {
    std::vector<std::string> chars;
    for (std::size_t pos = 0; pos < s.size();) {
        const std::size_t next = utf8_next(s, pos);
        chars.push_back(s.substr(pos, next - pos));
        pos = next;
    }
    return chars;
}
