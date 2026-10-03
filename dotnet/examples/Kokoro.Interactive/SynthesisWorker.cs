using System.Collections.Concurrent;
using System.Diagnostics;
using Kokoro.Net;

namespace Kokoro.Interactive;

/// <summary>
/// Owns the context until the session joins this thread: the C API forbids concurrent use of one context, and the
/// main thread must stay free to read stdin.
/// </summary>
internal sealed class SynthesisWorker(KokoroContext context, BlockingCollection<Phrase> queue, ClipPlayer player)
{
    public void Run()
    {
        foreach (var phrase in queue.GetConsumingEnumerable())
        {
            var stopwatch = Stopwatch.StartNew();
            try
            {
                var audio = context.Synthesize(phrase.Text, phrase.Settings.Voice, phrase.Settings.Speed,
                    phrase.Settings.Flags);
                stopwatch.Stop();
                var audio_s = audio.SampleRate > 0 ? audio.Samples.Length / (double)audio.SampleRate : 0;
                Console.Error.WriteLine($"Synthesized {audio_s:F2} s audio in {stopwatch.Elapsed.TotalSeconds:F3} s");
                player.Enqueue(audio);
            }
            catch (KokoroException e)
            {
                Console.Error.WriteLine($"kokoro: {e.Message}");
            }
            catch (InvalidOperationException e)
            {
                Console.Error.WriteLine($"kokoro: {e.Message}");
            }
        }
    }
}
