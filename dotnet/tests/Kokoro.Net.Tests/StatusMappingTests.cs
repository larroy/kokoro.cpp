using Xunit;

namespace Kokoro.Net.Tests;

public class StatusMappingTests
{
    public static TheoryData<KokoroStatus> FailingStatuses()
    {
        var data = new TheoryData<KokoroStatus>();
        foreach (var status in Enum.GetValues<KokoroStatus>().Where(s => s != KokoroStatus.Ok))
        {
            data.Add(status);
        }

        data.Add((KokoroStatus)99);
        return data;
    }

    [Theory]
    [MemberData(nameof(FailingStatuses))]
    public void FailedStatusThrowsWithStatusAndLastError(KokoroStatus status)
    {
        KokoroStatus Fake() => status;

        var ex = Assert.Throws<KokoroException>(() => KokoroStatusCheck.ThrowIfFailed(Fake(), () => $"fake {status}"));

        Assert.Equal(status, ex.Status);
        Assert.Equal($"fake {status}", ex.Message);
    }

    [Fact]
    public void Ok_DoesNotThrowOrReadLastError()
    {
        KokoroStatusCheck.ThrowIfFailed(KokoroStatus.Ok, () => throw new InvalidOperationException("lastError read"));
    }

    [Theory]
    [InlineData(KokoroStatus.Ok, 0)]
    [InlineData(KokoroStatus.InvalidArgument, 1)]
    [InlineData(KokoroStatus.Load, 2)]
    [InlineData(KokoroStatus.VoiceNotFound, 3)]
    [InlineData(KokoroStatus.Inference, 4)]
    [InlineData(KokoroStatus.OutOfMemory, 5)]
    [InlineData(KokoroStatus.Unknown, 6)]
    public void StatusValuesMatchHeader(KokoroStatus status, int expected)
    {
        Assert.Equal(expected, (int)status);
    }
}
