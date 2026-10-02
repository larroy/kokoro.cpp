using System.Collections.Concurrent;
using System.Runtime.CompilerServices;
using Xunit;

namespace Kokoro.Net.Tests;

public class SafeHandleTests
{
    private const int HandleCount = 1000;

    [Fact]
    public async Task ConcurrentDisposeReleasesEachHandleOnce()
    {
        var destroyed = new ConcurrentDictionary<IntPtr, int>();
        var handles = CreateHandles(destroyed, HandleCount);

        var disposals = handles.SelectMany(h => Enumerable.Range(0, 8).Select(_ => Task.Run(() => h.Dispose())));
        await Task.WhenAll(disposals);

        AssertEachDestroyedOnce(destroyed, HandleCount);
    }

    [Fact]
    public async Task DisposeDuringInFlightCallsDefersRelease()
    {
        const int calls = 10_000;
        int inFlight = 0;
        int started = 0;
        int destroyCount = 0;
        int inFlightAtRelease = -1;
        var handle = new KokoroContextHandle((IntPtr)1, _ =>
        {
            Interlocked.Increment(ref destroyCount);
            inFlightAtRelease = Volatile.Read(ref inFlight);
        });

        void Call()
        {
            if (Interlocked.Increment(ref started) == calls / 2)
            {
                handle.Dispose();
            }

            bool added = false;
            try
            {
                handle.DangerousAddRef(ref added);
            }
            catch (ObjectDisposedException)
            {
                return; // rejected: the handle was already closed
            }

            Interlocked.Increment(ref inFlight);
            Thread.SpinWait(50);
            Interlocked.Decrement(ref inFlight);
            handle.DangerousRelease();
        }

        await Task.WhenAll(Enumerable.Range(0, calls).Select(_ => Task.Run(Call)));

        Assert.Equal(1, Volatile.Read(ref destroyCount));
        Assert.Equal(0, inFlightAtRelease);
    }

    [Fact]
    public void AbandonedHandlesAreReleasedByFinalizer()
    {
        var destroyed = new ConcurrentDictionary<IntPtr, int>();
        CreateAndDropHandles(destroyed, HandleCount);

        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();

        AssertEachDestroyedOnce(destroyed, HandleCount);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void CreateAndDropHandles(ConcurrentDictionary<IntPtr, int> destroyed, int count) =>
        CreateHandles(destroyed, count);

    private static List<KokoroContextHandle> CreateHandles(ConcurrentDictionary<IntPtr, int> destroyed, int count) =>
        Enumerable.Range(1, count)
            .Select(i => new KokoroContextHandle((IntPtr)i, p => destroyed.AddOrUpdate(p, 1, (_, n) => n + 1)))
            .ToList();

    private static void AssertEachDestroyedOnce(ConcurrentDictionary<IntPtr, int> destroyed, int count)
    {
        Assert.Equal(count, destroyed.Count);
        for (int i = 1; i <= count; i++)
        {
            Assert.Equal(1, destroyed[(IntPtr)i]);
        }
    }
}
