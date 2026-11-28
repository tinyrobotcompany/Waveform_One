namespace Waveform.Certificates.Domain.Tests;

public sealed class DeviceRequestPolicyTests
{
    [Theory]
    [InlineData(-301, false)]
    [InlineData(-300, true)]
    [InlineData(0, true)]
    [InlineData(300, true)]
    [InlineData(301, false)]
    public void TimestampAllowsAtMostFiveMinutesOfClockSkew(long offset, bool accepted) =>
        Assert.Equal(accepted, DeviceRequestPolicy.IsTimestampFresh(1_800_000_000 + offset, 1_800_000_000));

    [Fact]
    public void TimestampExtremesDoNotOverflow()
    {
        Assert.False(DeviceRequestPolicy.IsTimestampFresh(long.MinValue, long.MaxValue));
        Assert.False(DeviceRequestPolicy.IsTimestampFresh(long.MaxValue, long.MinValue));
    }

    [Theory]
    [InlineData("10.0.0.1", true)]
    [InlineData("172.16.0.1", true)]
    [InlineData("172.31.255.255", true)]
    [InlineData("192.168.1.2", true)]
    [InlineData("172.15.255.255", false)]
    [InlineData("172.32.0.0", false)]
    [InlineData("8.8.8.8", false)]
    [InlineData("127.0.0.1", false)]
    [InlineData("169.254.1.1", false)]
    [InlineData("100.64.0.1", false)]
    [InlineData("::ffff:192.168.1.2", false)]
    [InlineData("10.1", false)]
    [InlineData("010.0.0.1", false)]
    [InlineData("0x0a000001", false)]
    [InlineData(" 10.0.0.1", false)]
    public void OnlyCanonicalRfc1918Ipv4AddressesAreAccepted(string address, bool accepted) =>
        Assert.Equal(accepted, DeviceRequestPolicy.IsPrivateAddress(address));

    [Fact]
    public void HostnameIsBoundToIdentifierAndConfiguredNamespace()
    {
        const string id = "0123456789abcdef0123456789abcdef";
        Assert.Equal($"d-{id}.waveform.example.test", DeviceRequestPolicy.Hostname(id, "waveform.example.test"));
        Assert.Throws<ArgumentException>(() => DeviceRequestPolicy.Hostname(id.ToUpperInvariant(), "waveform.example.test"));
        Assert.Throws<ArgumentException>(() => DeviceRequestPolicy.Hostname(id, "*.example.test"));
        Assert.Throws<ArgumentException>(() => DeviceRequestPolicy.Hostname(id, "bad..test"));
        Assert.Throws<ArgumentException>(() => DeviceRequestPolicy.Hostname(id, "-bad.test"));
    }
}
