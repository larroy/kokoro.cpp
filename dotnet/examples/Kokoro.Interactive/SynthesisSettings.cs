using Kokoro.Net;

namespace Kokoro.Interactive;

internal readonly record struct SynthesisSettings(string Voice, float Speed, SynthesisFlags Flags);

/// <summary>Settings are copied when queued: a later /voice does not change queued phrases.</summary>
internal readonly record struct Phrase(string Text, SynthesisSettings Settings);
