// Interactive mode: read phrases from stdin, synthesize and play them as they are produced.
#ifndef KOKORO_CLI_INTERACTIVE_H
#define KOKORO_CLI_INTERACTIVE_H

#include <kokoro/kokoro.h>

#include <string>

namespace kokoro_cli {

struct SynthesisSettings {
    std::string voice;
    float speed;
    unsigned flags;
};

// Reads phrases/commands from stdin until EOF or /quit. Returns the process exit status (always 0).
int run_interactive(kokoro_ctx* ctx, const SynthesisSettings& initial);

}  // namespace kokoro_cli

#endif  // KOKORO_CLI_INTERACTIVE_H
