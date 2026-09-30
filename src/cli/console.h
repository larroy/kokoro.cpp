// Console and stdin helpers: UTF-8 output on the Windows console, UTF-8 command-line arguments,
// and UTF-8 line input (via ReadConsoleW when stdin is a console, so non-ASCII input survives).
#ifndef KOKORO_CLI_CONSOLE_H
#define KOKORO_CLI_CONSOLE_H

#include <string>
#include <vector>

namespace kokoro_cli {

// Makes stdout/stderr accept UTF-8 (Windows console); no-op elsewhere.
void init_console();

// Command-line arguments as UTF-8 (re-read as UTF-16 on Windows).
std::vector<std::string> utf8_args(int argc, char** argv);

// Reads one line of stdin as UTF-8, without "\n"/"\r\n". Returns false at end of input.
bool read_line_utf8(std::string& line);

}  // namespace kokoro_cli

#endif  // KOKORO_CLI_CONSOLE_H
