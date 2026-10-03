using System.Text;

namespace Kokoro.Interactive;

/// <summary>
/// Reads lines from the console with UTF-8 output and, on a real console, UTF-16 input (ReadConsoleW), so Chinese
/// and Spanish input survive; piped input is read as UTF-8.
/// </summary>
internal sealed class ConsoleLineReader
{
    private readonly TextReader _reader;
    private readonly bool _disposeReader;

    private ConsoleLineReader(TextReader reader, bool disposeReader)
    {
        _reader = reader;
        _disposeReader = disposeReader;
    }

    public static ConsoleLineReader Open()
    {
        Console.OutputEncoding = new UTF8Encoding(false);
        if (Console.IsInputRedirected)
        {
            // Piped input is UTF-8; the BOM, if any, is stripped by the decoder.
            var stdin = Console.OpenStandardInput();
            return new ConsoleLineReader(new StreamReader(stdin, new UTF8Encoding(false)), true);
        }
        // Setting Unicode (code page 1200) makes .NET read the console with ReadConsoleW instead of the ANSI code
        // page, and skips SetConsoleCP, so no console settings are changed.
        Console.InputEncoding = Encoding.Unicode;
        return new ConsoleLineReader(Console.In, false);
    }

    /// <summary>Returns null at end of input, or on a line starting with Ctrl-Z (Ctrl-Z Enter on the console).</summary>
    public string? ReadLine()
    {
        var line = _reader.ReadLine();
        if (line is null)
        {
            return null;
        }
        return line.Length > 0 && line[0] == '\u001a' ? null : line;
    }
}
