/* Compiled as C: proves kokoro.h is a valid C header. */
#include <kokoro/kokoro.h>

const char* kokoro_c_header_version(void) {
    kokoro_audio audio = {0};
    kokoro_status status = KOKORO_OK;
    kokoro_number_language language = KOKORO_NUMBERS_AUTO;
    (void)audio;
    (void)status;
    (void)language;
    kokoro_language lang = KOKORO_LANGUAGE_AUTO;
    (void)lang;
    return kokoro_version();
}
