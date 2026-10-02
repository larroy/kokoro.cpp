using Microsoft.Win32.SafeHandles;

namespace Kokoro.Net;

/// <summary>Owns a <c>kokoro_ctx*</c>; releases it exactly once, after the last in-flight native call.</summary>
internal sealed class KokoroContextHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    private readonly Action<IntPtr> _destroy;

    /// <summary>Wraps <paramref name="ctx"/> and frees it with <c>kokoro_destroy</c>.</summary>
    internal KokoroContextHandle(IntPtr ctx) : this(ctx, KokoroNative.kokoro_destroy)
    {
    }

    /// <summary>Wraps <paramref name="ctx"/> and frees it with <paramref name="destroy"/>.</summary>
    internal KokoroContextHandle(IntPtr ctx, Action<IntPtr> destroy) : base(ownsHandle: true)
    {
        _destroy = destroy;
        SetHandle(ctx);
    }

    /// <inheritdoc />
    protected override bool ReleaseHandle()
    {
        _destroy(handle);
        return true;
    }
}
