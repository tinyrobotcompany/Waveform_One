using System.Net;
using System.Net.Sockets;

namespace Waveform.Certificates.Domain;

public static class DeviceRequestPolicy
{
    public static bool IsTimestampFresh(long issuedAt, long now) => Math.Abs((decimal)issuedAt - now) <= 300;

    public static bool IsPrivateAddress(string address)
    {
        if (!IPAddress.TryParse(address, out var ip) || ip.AddressFamily != AddressFamily.InterNetwork ||
            ip.ToString() != address) return false;
        var bytes = ip.GetAddressBytes();
        return bytes[0] == 10 || (bytes[0] == 172 && bytes[1] is >= 16 and <= 31) ||
               (bytes[0] == 192 && bytes[1] == 168);
    }

    public static string Hostname(string identifier, string suffix)
    {
        if (identifier is null || identifier.Length != 32 || !identifier.All(IsLowerHex))
            throw new ArgumentException("invalid_device_id");
        if (string.IsNullOrEmpty(suffix) || suffix.Length > 60 || !suffix.Contains('.') ||
            !suffix.Split('.').All(label => label.Length is >= 1 and <= 63 &&
                label[0] != '-' && label[^1] != '-' && label.All(c => c is >= 'a' and <= 'z' or >= '0' and <= '9' or '-')))
            throw new ArgumentException("invalid_device_suffix");
        return $"d-{identifier}.{suffix}";
    }

    internal static bool IsLowerHex(char value) => value is >= '0' and <= '9' or >= 'a' and <= 'f';
}
