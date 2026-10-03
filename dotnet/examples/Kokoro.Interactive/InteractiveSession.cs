using System.Collections.Concurrent;
using Kokoro.Net;

namespace Kokoro.Interactive;

/// <summary>
/// Reads phrases from stdin, synthesizes them on a worker thread and plays them in order; / commands change
/// settings for later phrases. The context is used only by the worker until it is joined.
/// </summary>
internal static class InteractiveSession
{
    public static int Run(KokoroContext context, SynthesisSettings initial, ConsoleLineReader input)
    {
        var voices = Enumerable.Range(0, context.VoiceCount).Select(context.VoiceName).ToArray();
        var commands = new SessionCommands(voices, initial);

        using var player = new ClipPlayer();
        using var queue = new BlockingCollection<Phrase>();
        var worker = new Thread(() => new SynthesisWorker(context, queue, player).Run())
        {
            Name = "kokoro-synthesis",
        };
        worker.Start();
        Console.Error.WriteLine("Interactive mode: type a phrase and press Enter to hear it; /help lists commands.");

        while (true)
        {
            Console.Error.Write("> ");
            var line = input.ReadLine();
            if (line is null)
            {
                Console.Error.WriteLine();
                queue.CompleteAdding();
                worker.Join();
                player.WaitIdle();
                return 0;
            }
            var text = line.Trim(' ', '\t', '\r', '\n');
            if (text.Length == 0)
            {
                continue;
            }
            if (text[0] == '/')
            {
                if (commands.Handle(text) == SessionAction.Quit)
                {
                    while (queue.TryTake(out _))
                    {
                    }
                    queue.CompleteAdding();
                    worker.Join();  // waits for the synthesis in progress, if any
                    player.Clear();
                    return 0;
                }
                continue;
            }
            queue.Add(new Phrase(text, commands.Settings));
        }
    }
}
