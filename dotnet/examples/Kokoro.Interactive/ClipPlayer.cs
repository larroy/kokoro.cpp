using Kokoro.Net;
using NAudio;
using NAudio.Wave;

namespace Kokoro.Interactive;

/// <summary>
/// Plays synthesized clips in order on one WaveOut device, opened lazily at the first clip's sample rate. Enqueue
/// is called from the synthesis thread; WaitIdle/Clear/Dispose from the main thread after it joins the worker.
/// </summary>
internal sealed class ClipPlayer : IDisposable
{
    private ClipQueueProvider? _provider;
    private WaveOut? _device;

    public void Enqueue(KokoroAudio audio)
    {
        if (audio.Samples.Length == 0)
        {
            return;
        }
        if (_device is null)
        {
            var provider = new ClipQueueProvider(audio.SampleRate);
            var device = new WaveOut();
            try
            {
                device.Init(provider);
                device.Play();
            }
            catch (MmException e)
            {
                device.Dispose();
                throw new InvalidOperationException($"cannot open audio device: {e.Message}", e);
                // _provider/_device stay null, so the next phrase retries the open.
            }
            _provider = provider;
            _device = device;
        }
        else if (audio.SampleRate != _provider!.WaveFormat.SampleRate)
        {
            throw new InvalidOperationException(
                $"sample rate changed from {_provider.WaveFormat.SampleRate} to {audio.SampleRate} Hz");
        }
        _provider!.Add(audio.Samples);  // no copy: KokoroAudio already owns a managed array
    }

    public void WaitIdle()
    {
        if (_provider is null)
        {
            return;
        }
        var device = _device!;
        _provider.WaitIdle(() => device.PlaybackState == PlaybackState.Playing);
    }

    public void Clear() => _provider?.Clear();

    public void Dispose() => _device?.Dispose();
}
