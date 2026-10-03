using Kokoro.Net;

namespace Kokoro.Interactive;

internal sealed record CliOptions(string Model, string Voices, string Dict, string Voice, float Speed,
    KokoroNumberLanguage NumberLanguage, KokoroLanguage Language, KokoroDevice Device, int GpuId,
    bool InputPhonemes, bool Help)
{
    public static CliOptions Default { get; } = new("models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin",
        "dict", "af_maple", 1.0f, KokoroNumberLanguage.Auto, KokoroLanguage.Auto, KokoroDevice.Auto, 0, false,
        false);
}
