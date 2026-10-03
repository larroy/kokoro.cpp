using System.Runtime.InteropServices;
using NAudio.Wave;

namespace Kokoro.Interactive;

/// <summary>
/// Feeds a WaveOut device from a queue of clips. Unlike BufferedWaveProvider there is no fixed capacity, so
/// arbitrarily long phrases fit. Read always fills the whole buffer and returns its length (returning 0 would
/// stop WaveOut); silence after the queue empties is counted so WaitIdle can tell playing from idle.
/// </summary>
internal sealed class ClipQueueProvider : IWaveProvider
{
    private const double TailSilence_s = 0.2;

    private readonly object _gate = new();  // Monitor, not Lock: WaitIdle needs Monitor.Wait
    private readonly Queue<float[]> _clips = [];
    private int _cursor;
    private long _trailingSilenceFrames;

    public WaveFormat WaveFormat { get; }

    private int TailFrames => (int)(WaveFormat.SampleRate * TailSilence_s);

    public ClipQueueProvider(int sampleRate)
    {
        WaveFormat = WaveFormat.CreateIeeeFloatWaveFormat(sampleRate, 1);
    }

    public int Read(Span<byte> buffer)
    {
        var output = MemoryMarshal.Cast<byte, float>(buffer);
        lock (_gate)
        {
            var written = 0;
            while (written < output.Length && _clips.Count > 0)
            {
                var clip = _clips.Peek();
                var take = Math.Min(clip.Length - _cursor, output.Length - written);
                clip.AsSpan(_cursor, take).CopyTo(output[written..]);
                written += take;
                _cursor += take;
                if (_cursor == clip.Length)
                {
                    _clips.Dequeue();
                    _cursor = 0;
                }
            }
            if (_clips.Count == 0)
            {
                // Silence past the queue signals idleness; only full silence counts toward it.
                _trailingSilenceFrames = (written > 0 ? 0 : _trailingSilenceFrames) + (output.Length - written);
                if (_trailingSilenceFrames >= TailFrames)
                {
                    Monitor.PulseAll(_gate);
                }
            }
            else
            {
                _trailingSilenceFrames = 0;
            }
            output[written..].Clear();
        }
        buffer[(output.Length * sizeof(float))..].Clear();  // trailing bytes when buffer.Length % 4 != 0
        return buffer.Length;
    }

    public void Add(float[] samples)
    {
        lock (_gate)
        {
            _clips.Enqueue(samples);
            _trailingSilenceFrames = 0;
        }
    }

    public void Clear()
    {
        lock (_gate)
        {
            _clips.Clear();
            _cursor = 0;
        }
    }

    /// <summary>Blocks until the queue is drained and a tail of silence has played, or isPlaying() says the
    /// device is dead (an unplugged device must not hang exit).</summary>
    public void WaitIdle(Func<bool> isPlaying)
    {
        lock (_gate)
        {
            while (true)
            {
                if (_clips.Count == 0 && _trailingSilenceFrames >= TailFrames || !isPlaying())
                {
                    return;
                }
                Monitor.Wait(_gate, TimeSpan.FromMilliseconds(100));
            }
        }
    }
}
