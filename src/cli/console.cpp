#include "console.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <iostream>

namespace kokoro_cli {

namespace {

#ifdef _WIN32
std::string wide_to_utf8(const wchar_t* wide) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string utf8(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &utf8[0], len, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(len) - 1);  // drop the terminator WideCharToMultiByte wrote
    return utf8;
}
#endif

}  // namespace

void init_console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

std::vector<std::string> utf8_args(int argc, char** argv) {
#ifdef _WIN32
    // The library takes UTF-8. argv is in the ANSI code page and loses characters outside it, so
    // re-read the command line as UTF-16.
    int count = 0;
    if (LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count)) {
        std::vector<std::string> args;
        args.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) args.push_back(wide_to_utf8(wide[i]));
        LocalFree(wide);
        return args;
    }
#endif
    return std::vector<std::string>(argv, argv + argc);
}

bool read_line_utf8(std::string& line) {
    line.clear();
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &mode)) {
        // A console: std::getline would return ANSI-code-page bytes and lose Chinese input, so
        // read UTF-16 with ReadConsoleW and convert.
        std::wstring wide;
        wchar_t buffer[512];
        DWORD read = 0;
        for (;;) {
            if (!ReadConsoleW(input, buffer, 512, &read, nullptr) || read == 0) return false;
            wide.append(buffer, read);
            if (wide.find(L'\n') != std::wstring::npos) break;
        }
        if (!wide.empty() && wide[0] == L'\x1a') return false;  // Ctrl-Z Enter
        while (!wide.empty() && (wide.back() == L'\n' || wide.back() == L'\r')) wide.pop_back();
        line = wide_to_utf8(wide.c_str());
        return true;
    }
#endif
    // Piped input (and every non-Windows platform) is taken as UTF-8.
    if (!std::getline(std::cin, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

}  // namespace kokoro_cli
