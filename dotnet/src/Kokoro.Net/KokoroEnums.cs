namespace Kokoro.Net;

/// <summary>Result of a kokoro C API call (<c>kokoro_status</c>).</summary>
public enum KokoroStatus
{
    /// <summary>The call succeeded.</summary>
    Ok = 0,

    /// <summary>NULL pointer or out-of-range value.</summary>
    InvalidArgument = 1,

    /// <summary>Model, voices, vocab or dictionary could not be loaded.</summary>
    Load = 2,

    /// <summary>The requested voice is not in the voices file.</summary>
    VoiceNotFound = 3,

    /// <summary>ONNX Runtime failed while synthesizing.</summary>
    Inference = 4,

    /// <summary>The library ran out of memory.</summary>
    OutOfMemory = 5,

    /// <summary>Any other failure.</summary>
    Unknown = 6,
}

/// <summary>Where inference runs (<c>kokoro_device</c>).</summary>
public enum KokoroDevice
{
    /// <summary>CUDA if this build's ONNX Runtime supports it and it initializes, else CPU.</summary>
    Auto = 0,

    /// <summary>Run on the CPU.</summary>
    Cpu = 1,

    /// <summary>Run on CUDA; fail with <see cref="KokoroStatus.Load"/> instead of falling back to CPU.</summary>
    Cuda = 2,
}

/// <summary>How G2P reads digits (<c>kokoro_number_language</c>).</summary>
public enum KokoroNumberLanguage
{
    /// <summary>
    /// Spanish text (<see cref="KokoroContext.SetLanguage"/>): Spanish. Chinese/English text: per number, the
    /// language of the nearest letter or CJK character; Chinese if none.
    /// </summary>
    Auto = 0,

    /// <summary>Read digits as English words.</summary>
    English = 1,

    /// <summary>Read digits as Chinese words.</summary>
    Chinese = 2,

    /// <summary>Spanish words; in Chinese/English text they are read by the English G2P.</summary>
    Spanish = 3,
}

/// <summary>Which G2P reads text (<c>kokoro_language</c>).</summary>
public enum KokoroLanguage
{
    /// <summary>
    /// Synthesize: Spanish for voices named ef_* or em_*, else Chinese and English by script;
    /// phonemize: Chinese and English by script.
    /// </summary>
    Auto = 0,

    /// <summary>All text is read as Spanish, numbers in Spanish.</summary>
    Spanish = 1,
}

/// <summary>Flags for <see cref="KokoroContext.Synthesize"/>.</summary>
[Flags]
public enum SynthesisFlags : uint
{
    /// <summary>Text is plain text and goes through G2P.</summary>
    None = 0,

    /// <summary>Text is already a phoneme string; skip G2P.</summary>
    InputPhonemes = 0x1,
}
