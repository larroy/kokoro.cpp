namespace Kokoro.Net;

/// <summary>A kokoro C API call failed; the message is <c>kokoro_last_error()</c> at the time of failure.</summary>
public sealed class KokoroException : Exception
{
    /// <summary>Creates an exception for a failed call.</summary>
    /// <param name="status">Status the native call returned.</param>
    /// <param name="message">Error message reported by the library.</param>
    public KokoroException(KokoroStatus status, string message) : base(message)
    {
        Status = status;
    }

    /// <summary>Status the native call returned.</summary>
    public KokoroStatus Status { get; }
}
