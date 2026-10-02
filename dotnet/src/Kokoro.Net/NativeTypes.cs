using System.Runtime.InteropServices;

namespace Kokoro.Net;

/// <summary>Mirror of <c>kokoro_audio</c>: mono float PCM owned by the library.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeAudio
{
    public IntPtr samples;
    public UIntPtr numSamples;
    public int sampleRate;
}

/// <summary>Mirror of <c>kokoro_options</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeOptions
{
    public KokoroDevice device;
    public int gpuId;
}
