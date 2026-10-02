using System.Reflection;
using Xunit;

namespace Kokoro.Net.Tests;

public class Utf8MarshalingTests
{
    [Theory]
    [InlineData("missing-model.onnx")]
    [InlineData("缺失的模型-你好.onnx")]
    [InlineData("modèle-café-ñandú.onnx")]
    public void MissingModelMessageRoundTripsUtf8(string path)
    {
        var ex = Assert.Throws<KokoroException>(() => new KokoroContext(path, "missing-voices.bin", "missing-dict"));

        Assert.Equal(KokoroStatus.Load, ex.Status);
        Assert.Equal("Model file not found: " + path, ex.Message);
    }

    [Fact]
    public void NativeVersionMatchesPackageVersion()
    {
        var expected = typeof(KokoroContext).Assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()!.InformationalVersion;

        Assert.Equal(expected, KokoroContext.Version());
    }
}
