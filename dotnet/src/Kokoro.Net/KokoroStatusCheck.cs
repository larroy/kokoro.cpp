using System.Runtime.InteropServices;

namespace Kokoro.Net;

/// <summary>Turns a failed <see cref="KokoroStatus"/> into a <see cref="KokoroException"/>.</summary>
internal static class KokoroStatusCheck
{
    /// <summary>Throws when <paramref name="status"/> is not Ok; <paramref name="lastError"/> runs only then.</summary>
    internal static void ThrowIfFailed(KokoroStatus status, Func<string> lastError)
    {
        if (status != KokoroStatus.Ok)
        {
            throw new KokoroException(status, lastError());
        }
    }

    /// <summary>Throws with <c>kokoro_last_error()</c> as the message when <paramref name="status"/> fails.</summary>
    internal static void ThrowIfFailed(KokoroStatus status) => ThrowIfFailed(status, LastError);

    /// <summary>The calling thread's <c>kokoro_last_error()</c> message.</summary>
    internal static string LastError() => Marshal.PtrToStringUTF8(KokoroNative.kokoro_last_error()) ?? string.Empty;
}
