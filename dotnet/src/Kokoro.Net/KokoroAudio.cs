namespace Kokoro.Net;

/// <summary>Synthesized mono float PCM.</summary>
/// <param name="Samples">Mono samples, nominally in [-1, 1].</param>
/// <param name="SampleRate">Samples per second.</param>
public sealed record KokoroAudio(float[] Samples, int SampleRate);
