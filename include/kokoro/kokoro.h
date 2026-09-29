/*
 * kokoro.cpp C API.
 *
 * Strings are NUL-terminated UTF-8. Every function that can fail returns a
 * kokoro_status; on failure kokoro_last_error() describes the cause.
 *
 * Threading: separate contexts are independent and may be used from different
 * threads. A single context must not be used by two threads at the same time.
 */
#ifndef KOKORO_KOKORO_H
#define KOKORO_KOKORO_H

#include <stddef.h>

#if defined(_WIN32)
#  if defined(KOKORO_BUILD)
#    define KOKORO_API __declspec(dllexport)
#  else
#    define KOKORO_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define KOKORO_API __attribute__((visibility("default")))
#else
#  define KOKORO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kokoro_status {
    KOKORO_OK = 0,
    KOKORO_ERROR_INVALID_ARGUMENT = 1, /* NULL pointer or out-of-range value */
    KOKORO_ERROR_LOAD = 2,             /* model, voices, vocab or dictionary could not be loaded */
    KOKORO_ERROR_VOICE_NOT_FOUND = 3,
    KOKORO_ERROR_INFERENCE = 4,        /* ONNX Runtime failed while synthesizing */
    KOKORO_ERROR_OUT_OF_MEMORY = 5,
    KOKORO_ERROR_UNKNOWN = 6
} kokoro_status;

/* kokoro_synthesize flags. */
#define KOKORO_INPUT_PHONEMES 0x1u /* text is already a phoneme string; skip G2P */

typedef struct kokoro_ctx kokoro_ctx;

/* Mono float PCM owned by the library; release with kokoro_audio_free(). */
typedef struct kokoro_audio {
    float* samples;
    size_t num_samples;
    int sample_rate;
} kokoro_audio;

/* Library version, e.g. "0.1.0". */
KOKORO_API const char* kokoro_version(void);

/*
 * Message for the most recent failed call on the calling thread, or "" if none.
 * Valid until the next kokoro_* call on the same thread.
 */
KOKORO_API const char* kokoro_last_error(void);

/*
 * Loads the ONNX model, the voices file and the dictionaries in dict_dir
 * (vocab.txt, jieba, pinyin, CMU and g2p_en files; see dict/ in the repo).
 * On success stores the new context in *out_ctx.
 */
KOKORO_API kokoro_status kokoro_create(const char* model_path, const char* voices_path, const char* dict_dir,
                                       kokoro_ctx** out_ctx);

/* Frees the context. NULL is ignored. */
KOKORO_API void kokoro_destroy(kokoro_ctx* ctx);

/* Number of voices in the loaded voices file. */
KOKORO_API size_t kokoro_voice_count(const kokoro_ctx* ctx);

/* Name of voice `index` (sorted), or NULL if out of range. Valid for the context's lifetime. */
KOKORO_API const char* kokoro_voice_name(const kokoro_ctx* ctx, size_t index);

/*
 * Synthesizes `text` with `voice` at `speed` (1.0 = normal, must be > 0).
 * flags: 0 or KOKORO_INPUT_PHONEMES.
 * On success fills *out_audio; the caller must release it with kokoro_audio_free().
 * On failure *out_audio is zeroed.
 */
KOKORO_API kokoro_status kokoro_synthesize(kokoro_ctx* ctx, const char* text, const char* voice, float speed,
                                           unsigned int flags, kokoro_audio* out_audio);

/* Converts text to the phoneme string the model consumes. Release with kokoro_string_free(). */
KOKORO_API kokoro_status kokoro_phonemize(kokoro_ctx* ctx, const char* text, char** out_phonemes);

/* Frees samples and zeroes *audio. NULL is ignored. */
KOKORO_API void kokoro_audio_free(kokoro_audio* audio);

/* Frees a string returned by the library. NULL is ignored. */
KOKORO_API void kokoro_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif /* KOKORO_KOKORO_H */
