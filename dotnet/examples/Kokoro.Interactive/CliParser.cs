using System.Diagnostics;
using System.Globalization;
using Kokoro.Net;

namespace Kokoro.Interactive;

/// <summary>Parses command-line arguments; exactly one of Options and Error is non-null.</summary>
internal static class CliParser
{
    public sealed record ParseResult(CliOptions? Options, string? Error);

    public const string Usage =
        """
        Usage: Kokoro.Interactive [options]

        Read phrases from stdin, synthesize them with the Kokoro TTS model and play them as they are produced.

        Options:
          -m, --model <path>    ONNX model file (default: models/kokoro-v1.1-zh.onnx)
              --voices <path>   voices file (default: models/voices-v1.1-zh.bin)
          -d, --dict <dir>      dictionary directory (default: dict)
          -v, --voice <name>    initial voice (default: af_maple)
              --lang <auto|en|zh|es>  language for reading numbers (default: auto)
              --language <auto|es>  text language; auto: Spanish for ef_*/em_* voices, else Chinese/English (default: auto)
          -s, --speed <rate>    initial speaking rate, > 0 (default: 1.0)
              --device <name>   auto, cpu or cuda (default: auto)
              --gpu-id <n>      CUDA device to use (default: 0)
          -p, --phonemes        phrases are phoneme strings; skip G2P
          -h, --help            print this help and exit
        """;

    private enum Opt { Model, Voices, Dict, Voice, Lang, Language, Speed, Device, GpuId, Phonemes, Help }

    private readonly record struct OptionSpec(string? ShortName, string LongName, Opt Id, bool TakesValue);

    private static readonly OptionSpec[] OptionsTable =
    [
        new("-m", "--model", Opt.Model, true),
        new(null, "--voices", Opt.Voices, true),
        new("-d", "--dict", Opt.Dict, true),
        new("-v", "--voice", Opt.Voice, true),
        new(null, "--lang", Opt.Lang, true),
        new(null, "--language", Opt.Language, true),
        new("-s", "--speed", Opt.Speed, true),
        new(null, "--device", Opt.Device, true),
        new(null, "--gpu-id", Opt.GpuId, true),
        new("-p", "--phonemes", Opt.Phonemes, false),
        new("-h", "--help", Opt.Help, false),
    ];

    public static ParseResult Parse(IReadOnlyList<string> args)
    {
        var options = CliOptions.Default;
        for (var i = 0; i < args.Count; i++)
        {
            var arg = args[i];
            if (arg.Length < 2 || arg[0] != '-')
            {
                return new ParseResult(null, $"unexpected argument '{arg}'");
            }

            // Only long options accept an inline "=value"; "--" is not special (no positionals here).
            var name = arg;
            var value = "";
            var hasValue = false;
            if (arg.StartsWith("--", StringComparison.Ordinal))
            {
                var eq = arg.IndexOf('=');
                if (eq >= 0)
                {
                    name = arg[..eq];
                    value = arg[(eq + 1)..];
                    hasValue = true;
                }
            }

            var spec = FindOption(name);
            if (spec is null)
            {
                return new ParseResult(null, $"unknown option '{arg}'");
            }

            if (!spec.Value.TakesValue)
            {
                if (hasValue)
                {
                    return new ParseResult(null, $"option '{name}' does not take a value");
                }
            }
            else if (!hasValue)
            {
                if (i + 1 >= args.Count)
                {
                    return new ParseResult(null, $"option '{name}' requires a value");
                }
                value = args[++i];
            }

            if (spec.Value.Id == Opt.Help)
            {
                return new ParseResult(options with { Help = true }, null);
            }

            var applied = Apply(options, spec.Value.Id, value);
            if (applied.Error is not null)
            {
                return applied;
            }
            options = applied.Options!;
        }
        return new ParseResult(options, null);
    }

    private static OptionSpec? FindOption(string name)
    {
        foreach (var spec in OptionsTable)
        {
            if (name == spec.LongName || (spec.ShortName is not null && name == spec.ShortName))
            {
                return spec;
            }
        }
        return null;
    }

    private static ParseResult Apply(CliOptions options, Opt id, string value) => id switch
    {
        Opt.Model => new ParseResult(options with { Model = value }, null),
        Opt.Voices => new ParseResult(options with { Voices = value }, null),
        Opt.Dict => new ParseResult(options with { Dict = value }, null),
        Opt.Voice => new ParseResult(options with { Voice = value }, null),
        Opt.Speed => SpeedValue.TryParse(value, out var speed)
            ? new ParseResult(options with { Speed = speed }, null)
            : new ParseResult(null, $"invalid speed '{value}' (must be a number > 0)"),
        Opt.Lang => TryParseNumberLanguage(value, out var numberLanguage)
            ? new ParseResult(options with { NumberLanguage = numberLanguage }, null)
            : new ParseResult(null, $"invalid number language '{value}' (must be auto, en, zh or es)"),
        Opt.Language => TryParseLanguage(value, out var language)
            ? new ParseResult(options with { Language = language }, null)
            : new ParseResult(null, $"invalid language '{value}' (must be auto or es)"),
        Opt.Device => TryParseDevice(value, out var device)
            ? new ParseResult(options with { Device = device }, null)
            : new ParseResult(null, $"invalid device '{value}' (expected auto, cpu or cuda)"),
        Opt.GpuId => int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out var gpuId)
            ? new ParseResult(options with { GpuId = gpuId }, null)
            : new ParseResult(null, $"invalid GPU id '{value}' (must be an integer >= 0)"),
        Opt.Phonemes => new ParseResult(options with { InputPhonemes = true }, null),
        Opt.Help => new ParseResult(options with { Help = true }, null),
        _ => throw new UnreachableException(),
    };

    private static bool TryParseNumberLanguage(string value, out KokoroNumberLanguage language)
    {
        switch (value)
        {
            case "auto":
                language = KokoroNumberLanguage.Auto;
                return true;
            case "en":
                language = KokoroNumberLanguage.English;
                return true;
            case "zh":
                language = KokoroNumberLanguage.Chinese;
                return true;
            case "es":
                language = KokoroNumberLanguage.Spanish;
                return true;
            default:
                language = default;
                return false;
        }
    }

    private static bool TryParseLanguage(string value, out KokoroLanguage language)
    {
        switch (value)
        {
            case "auto":
                language = KokoroLanguage.Auto;
                return true;
            case "es":
                language = KokoroLanguage.Spanish;
                return true;
            default:
                language = default;
                return false;
        }
    }

    private static bool TryParseDevice(string value, out KokoroDevice device)
    {
        switch (value)
        {
            case "auto":
                device = KokoroDevice.Auto;
                return true;
            case "cpu":
                device = KokoroDevice.Cpu;
                return true;
            case "cuda":
                device = KokoroDevice.Cuda;
                return true;
            default:
                device = default;
                return false;
        }
    }
}
