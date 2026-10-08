using Kokoro.Net;

if (args.Length != 4)
{
    Console.Error.WriteLine("usage: KokoroSmoke <model.onnx> <voices.bin> <dict-dir> <expected-version>");
    return 2;
}

string version = KokoroContext.Version();
if (version != args[3])
{
    Console.Error.WriteLine($"native kokoro version {version} != expected {args[3]}");
    return 1;
}

string dictDir = args[2] == "bundled" ? KokoroContext.BundledDictDirectory : args[2];
using var ctx = new KokoroContext(args[0], args[1], dictDir, new KokoroOptions(KokoroDevice.Cpu, 0));
var audio = ctx.Synthesize("Hello world.", "af_maple");
Console.WriteLine($"kokoro {version} on {ctx.ContextDevice}: {audio.Samples.Length} samples at {audio.SampleRate} Hz");
return audio.Samples.Length > 0 && audio.SampleRate > 0 ? 0 : 1;
