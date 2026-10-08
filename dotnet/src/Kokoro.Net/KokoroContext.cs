using System.Runtime.InteropServices;

namespace Kokoro.Net;

/// <summary>A loaded Kokoro model, voices file and G2P dictionaries (<c>kokoro_ctx</c>).</summary>
/// <remarks>
/// Threading: separate contexts are independent and may be used from different threads. A single context must not
/// be used by two threads at the same time.
/// </remarks>
public sealed class KokoroContext : IDisposable
{
    private readonly KokoroContextHandle _handle;

    /// <summary>
    /// Loads the ONNX model, the voices file and the dictionaries in <paramref name="dictDir"/>
    /// (vocab.txt, jieba, pinyin, CMU and g2p_en files; see dict/ in the kokoro.cpp repo).
    /// </summary>
    /// <param name="modelPath">Path to the Kokoro ONNX model.</param>
    /// <param name="voicesPath">Path to the voices file.</param>
    /// <param name="dictDir">Directory holding the G2P dictionaries.</param>
    /// <param name="options">
    /// Device selection. With <see cref="KokoroDevice.Auto"/> a CUDA initialization failure is reported on stderr
    /// and the context runs on the CPU.
    /// </param>
    /// <exception cref="KokoroException">The native library could not create the context.</exception>
    public KokoroContext(string modelPath, string voicesPath, string dictDir, KokoroOptions options = default)
    {
        var native = new NativeOptions { device = options.Device, gpuId = options.GpuId };
        var status = KokoroNative.kokoro_create_ex(modelPath, voicesPath, dictDir, in native, out IntPtr ctx);
        KokoroStatusCheck.ThrowIfFailed(status);
        _handle = new KokoroContextHandle(ctx);
    }

    /// <summary>Native library version, e.g. "0.1.0".</summary>
    /// <returns>The value of <c>kokoro_version()</c>.</returns>
    public static string Version() => Marshal.PtrToStringUTF8(KokoroNative.kokoro_version())!;

    /// <summary>Directory where the Larroy.Kokoro package copies the G2P dictionaries (<c>&lt;app&gt;/kokoro-dict</c>).</summary>
    public static string BundledDictDirectory { get; } = Path.Combine(AppContext.BaseDirectory, "kokoro-dict");

    /// <summary>Number of voices in the loaded voices file.</summary>
    public int VoiceCount => checked((int)KokoroNative.kokoro_voice_count(_handle));

    /// <summary>
    /// Where this context runs inference: <see cref="KokoroDevice.Cpu"/> or <see cref="KokoroDevice.Cuda"/>.
    /// </summary>
    public KokoroDevice ContextDevice => KokoroNative.kokoro_context_device(_handle);

    /// <summary>Name of voice <paramref name="index"/>; voices are sorted by name.</summary>
    /// <param name="index">Zero-based voice index, below <see cref="VoiceCount"/>.</param>
    /// <returns>The voice name.</returns>
    /// <exception cref="ArgumentOutOfRangeException">No voice exists at <paramref name="index"/>.</exception>
    public string VoiceName(int index)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        IntPtr name = KokoroNative.kokoro_voice_name(_handle, (nuint)index);
        if (name == IntPtr.Zero)
        {
            throw new ArgumentOutOfRangeException(nameof(index), index, "No voice at this index.");
        }

        return Marshal.PtrToStringUTF8(name)!;
    }

    /// <summary>Synthesizes <paramref name="text"/> with <paramref name="voice"/>.</summary>
    /// <param name="text">Text to speak, or a phoneme string with <see cref="SynthesisFlags.InputPhonemes"/>.</param>
    /// <param name="voice">Voice name, e.g. "af_maple".</param>
    /// <param name="speed">Speaking rate; 1.0 is normal and it must be greater than 0.</param>
    /// <param name="flags">Input interpretation flags.</param>
    /// <returns>The synthesized mono float PCM.</returns>
    /// <exception cref="KokoroException">Synthesis failed.</exception>
    public KokoroAudio Synthesize(string text, string voice, float speed = 1.0f,
        SynthesisFlags flags = SynthesisFlags.None)
    {
        var status = KokoroNative.kokoro_synthesize(_handle, text, voice, speed, (uint)flags, out NativeAudio audio);
        try
        {
            KokoroStatusCheck.ThrowIfFailed(status);
            return new KokoroAudio(CopySamples(audio), audio.sampleRate);
        }
        finally
        {
            KokoroNative.kokoro_audio_free(ref audio);
        }
    }

    /// <summary>Converts <paramref name="text"/> to the phoneme string the model consumes.</summary>
    /// <param name="text">Text to convert.</param>
    /// <returns>The phoneme string.</returns>
    /// <exception cref="KokoroException">Phonemization failed.</exception>
    public string Phonemize(string text)
    {
        var status = KokoroNative.kokoro_phonemize(_handle, text, out IntPtr phonemes);
        try
        {
            KokoroStatusCheck.ThrowIfFailed(status);
            return Marshal.PtrToStringUTF8(phonemes)!;
        }
        finally
        {
            KokoroNative.kokoro_string_free(phonemes);
        }
    }

    /// <summary>Sets how digits are read by later synthesize/phonemize calls. Default: Auto.</summary>
    /// <param name="language">Number reading language.</param>
    /// <exception cref="KokoroException">The value was rejected.</exception>
    public void SetNumberLanguage(KokoroNumberLanguage language) =>
        KokoroStatusCheck.ThrowIfFailed(KokoroNative.kokoro_set_number_language(_handle, language));

    /// <summary>
    /// Sets the G2P language for later synthesize/phonemize calls. Default: Auto. A number language other than
    /// <see cref="KokoroNumberLanguage.Auto"/> also applies to Spanish text.
    /// </summary>
    /// <param name="language">G2P language.</param>
    /// <exception cref="KokoroException">The value was rejected.</exception>
    public void SetLanguage(KokoroLanguage language) =>
        KokoroStatusCheck.ThrowIfFailed(KokoroNative.kokoro_set_language(_handle, language));

    /// <summary>Frees the native context once no call is using it.</summary>
    public void Dispose() => _handle.Dispose();

    private static float[] CopySamples(in NativeAudio a)
    {
        int n = checked((int)a.numSamples);
        if (n == 0)
        {
            return [];
        }

        var dst = new float[n];
        Marshal.Copy(a.samples, dst, 0, n);
        return dst;
    }
}
