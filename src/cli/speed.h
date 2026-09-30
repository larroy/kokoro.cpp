// parse_speed: shared CLI helper for the -s/--speed option and the /speed command.
#ifndef KOKORO_CLI_SPEED_H
#define KOKORO_CLI_SPEED_H

#include <cmath>
#include <cstdlib>
#include <string>

namespace kokoro_cli {

// Parses a strictly positive finite decimal number. On success sets speed.
inline bool parse_speed(const std::string& value, float& speed) {
    if (value.empty()) return false;
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !std::isfinite(parsed) || !(parsed > 0.0f)) return false;
    speed = parsed;
    return true;
}

}  // namespace kokoro_cli

#endif  // KOKORO_CLI_SPEED_H
