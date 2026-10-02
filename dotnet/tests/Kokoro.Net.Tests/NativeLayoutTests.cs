using System.Runtime.InteropServices;
using Xunit;

namespace Kokoro.Net.Tests;

// Pins the managed mirrors to kokoro_audio and kokoro_options in include/kokoro/kokoro.h.
public class NativeLayoutTests
{
    [Fact]
    public void NativeAudioMatchesKokoroAudio()
    {
        Assert.Equal(3 * IntPtr.Size, Marshal.SizeOf<NativeAudio>());
        Assert.Equal(0, (int)Marshal.OffsetOf<NativeAudio>(nameof(NativeAudio.samples)));
        Assert.Equal(IntPtr.Size, (int)Marshal.OffsetOf<NativeAudio>(nameof(NativeAudio.numSamples)));
        Assert.Equal(2 * IntPtr.Size, (int)Marshal.OffsetOf<NativeAudio>(nameof(NativeAudio.sampleRate)));
    }

    [Fact]
    public void NativeOptionsMatchesKokoroOptions()
    {
        Assert.Equal(8, Marshal.SizeOf<NativeOptions>());
        Assert.Equal(0, (int)Marshal.OffsetOf<NativeOptions>(nameof(NativeOptions.device)));
        Assert.Equal(4, (int)Marshal.OffsetOf<NativeOptions>(nameof(NativeOptions.gpuId)));
    }
}
