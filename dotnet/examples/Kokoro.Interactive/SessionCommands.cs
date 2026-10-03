using System.Globalization;
using Kokoro.Net;

namespace Kokoro.Interactive;

/// <summary>Holds the settings that new phrases get and interprets / commands.</summary>
internal sealed class SessionCommands(IReadOnlyList<string> voices, SynthesisSettings initial)
{
    public SynthesisSettings Settings { get; private set; } = initial;

    /// <summary>Handles a trimmed line that starts with '/': the command up to the first space, the rest as the
    /// argument.</summary>
    public SessionAction Handle(string line)
    {
        var space = line.IndexOf(' ');
        var command = space < 0 ? line : line[..space];
        var argument = (space < 0 ? "" : line[(space + 1)..]).Trim(' ', '\t');
        return command switch
        {
            "/quit" => SessionAction.Quit,
            "/voices" => HandleVoices(),
            "/voice" => HandleVoice(argument),
            "/speed" => HandleSpeed(argument),
            "/help" => HandleHelp(),
            _ => UnknownCommand(command),
        };
    }

    private SessionAction HandleVoices()
    {
        foreach (var voice in voices)
        {
            Console.Out.WriteLine(voice);
        }
        return SessionAction.Continue;
    }

    private SessionAction HandleVoice(string name)
    {
        if (name.Length > 0)
        {
            if (!voices.Contains(name, StringComparer.Ordinal))
            {
                Console.Error.WriteLine($"kokoro: unknown voice '{name}'; try /voices");
                return SessionAction.Continue;
            }
            Settings = Settings with { Voice = name };
        }
        Console.Error.WriteLine($"voice: {Settings.Voice}");
        return SessionAction.Continue;
    }

    private SessionAction HandleSpeed(string argument)
    {
        if (argument.Length > 0)
        {
            if (!SpeedValue.TryParse(argument, out var speed))
            {
                Console.Error.WriteLine($"kokoro: invalid speed '{argument}' (must be a number > 0)");
                return SessionAction.Continue;
            }
            Settings = Settings with { Speed = speed };
        }
        Console.Error.WriteLine($"speed: {Settings.Speed.ToString(CultureInfo.InvariantCulture)}");
        return SessionAction.Continue;
    }

    private static SessionAction HandleHelp()
    {
        Console.Error.WriteLine(
            """
            Type a phrase and press Enter to hear it. Commands:
              /voice [name]   show or set the voice
              /speed [rate]   show or set the speaking rate (> 0)
              /voices         list the available voices
              /help           show this help
              /quit           stop immediately, discarding queued phrases
            End of input (Ctrl-Z Enter on Windows) quits after queued phrases finish playing.
            """);
        return SessionAction.Continue;
    }

    private static SessionAction UnknownCommand(string command)
    {
        Console.Error.WriteLine($"kokoro: unknown command '{command}'; try /help");
        return SessionAction.Continue;
    }
}
