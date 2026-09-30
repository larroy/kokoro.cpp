/* Compiled as C: proves kokoro.h is a valid C header. */
#include <kokoro/kokoro.h>

const char* kokoro_c_header_version(void) {
    kokoro_audio audio = {0};
    kokoro_status status = KOKORO_OK;
    (void)audio;
    (void)status;
    return kokoro_version();
}
