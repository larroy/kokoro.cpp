using System.Runtime.InteropServices;

namespace Kokoro.Net;

/// <summary>
/// P/Invoke declarations for include/kokoro/kokoro.h. Library-owned <c>const char*</c> returns stay
/// <see cref="IntPtr"/> so the marshaller never frees memory the library owns.
/// </summary>
internal static class KokoroNative
{
    internal const string Lib = "kokoro";

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern IntPtr kokoro_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern IntPtr kokoro_last_error();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_create(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modelPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string voicesPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string dictDir,
        out IntPtr outCtx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_create_ex(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modelPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string voicesPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string dictDir,
        in NativeOptions options,
        out IntPtr outCtx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroDevice kokoro_context_device(KokoroContextHandle ctx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern NativeOptions kokoro_default_options();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern void kokoro_destroy(IntPtr ctx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern UIntPtr kokoro_voice_count(KokoroContextHandle ctx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern IntPtr kokoro_voice_name(KokoroContextHandle ctx, UIntPtr index);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_synthesize(
        KokoroContextHandle ctx,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string voice,
        float speed,
        uint flags,
        out NativeAudio outAudio);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_phonemize(
        KokoroContextHandle ctx,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
        out IntPtr outPhonemes);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_set_number_language(
        KokoroContextHandle ctx,
        KokoroNumberLanguage language);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern KokoroStatus kokoro_set_language(KokoroContextHandle ctx, KokoroLanguage language);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern void kokoro_audio_free(ref NativeAudio audio);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    internal static extern void kokoro_string_free(IntPtr str);
}
