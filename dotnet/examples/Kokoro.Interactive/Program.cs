using System.Diagnostics;
using Kokoro.Net;

namespace Kokoro.Interactive;

internal static class Program
{
    private static int Main(string[] args)
    {
        var input = ConsoleLineReader.Open();

        var result = CliParser.Parse(args);
        if (result.Error is not null)
        {
            Console.Error.WriteLine($"kokoro: {result.Error}");
            Console.Error.WriteLine("Try 'Kokoro.Interactive --help'.");
            return 2;
        }
        var options = result.Options!;
        if (options.Help)
        {
            Console.Out.Write(CliParser.Usage);
            return 0;
        }

        var stopwatch = Stopwatch.StartNew();
        KokoroContext context;
        try
        {
            context = new KokoroContext(options.Model, options.Voices, options.Dict,
                new KokoroOptions(options.Device, options.GpuId));
            context.SetNumberLanguage(options.NumberLanguage);
            context.SetLanguage(options.Language);
        }
        catch (KokoroException e)
        {
            Console.Error.WriteLine($"kokoro: {e.Message}");
            return 1;
        }
        stopwatch.Stop();
        Console.Error.WriteLine($"Initialized engine in {stopwatch.Elapsed.TotalSeconds:F3} s");

        // The context is disposed after Run returns, i.e. after the worker thread was joined.
        using (context)
        {
            var flags = options.InputPhonemes ? SynthesisFlags.InputPhonemes : SynthesisFlags.None;
            return InteractiveSession.Run(context, new SynthesisSettings(options.Voice, options.Speed, flags),
                input);
        }
    }
}
