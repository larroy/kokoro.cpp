// Prints one phoneme line per stdin line, for compare_g2p.py.
#include "SpanishG2P.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "es") {
        std::cerr << "usage: g2p_dump <language>\n  language: es\n";
        return 2;
    }
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::cout << spanish_to_phonemes(line, NumberLanguage::Auto) << '\n';
    }
    return 0;
}
