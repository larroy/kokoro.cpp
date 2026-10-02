namespace Kokoro.Net;

/// <summary>
/// Context creation options (<c>kokoro_options</c>). <c>default</c> is <see cref="KokoroDevice.Auto"/> on GPU 0,
/// the same as <c>kokoro_default_options()</c>.
/// </summary>
/// <param name="Device">Where inference runs.</param>
/// <param name="GpuId">CUDA device ordinal, &gt;= 0; ignored on CPU.</param>
public readonly record struct KokoroOptions(KokoroDevice Device, int GpuId);
