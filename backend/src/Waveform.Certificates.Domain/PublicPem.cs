using System.Security.Cryptography;

namespace Waveform.Certificates.Domain;

internal static class PublicPem
{
    public static byte[] Decode(string pem, string label)
    {
        if (string.IsNullOrEmpty(pem) || !PemEncoding.TryFind(pem, out var fields) ||
            !pem.AsSpan(fields.Label).SequenceEqual(label) ||
            !pem.AsSpan(..fields.Location.Start.GetOffset(pem.Length)).IsWhiteSpace() ||
            !pem.AsSpan(fields.Location.End.GetOffset(pem.Length)..).IsWhiteSpace())
            throw new ArgumentException("invalid_pem");
        return Convert.FromBase64String(pem[fields.Base64Data]);
    }
}
