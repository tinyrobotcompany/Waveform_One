using System.Security.Cryptography;

namespace Waveform.Certificates.Domain;

public static class DeviceIdentity
{
    public static string Identifier(string publicKeyPem)
    {
        using var key = ReadPublicKey(publicKeyPem);
        return Convert.ToHexStringLower(SHA256.HashData(key.ExportSubjectPublicKeyInfo()))[..32];
    }

    // The caller supplies the complete, approved signing envelope and original body bytes.
    // This primitive deliberately does not choose an HTTP route, header or product context.
    public static bool VerifySignedBytes(string publicKeyPem, ReadOnlySpan<byte> signedBytes, string signature)
    {
        if (string.IsNullOrEmpty(signature) || signature.Length > 104 || signature.Any(char.IsWhiteSpace))
            return false;
        try
        {
            var der = Convert.FromBase64String(signature);
            using var key = ReadPublicKey(publicKeyPem);
            return key.VerifyData(signedBytes, der, HashAlgorithmName.SHA256, DSASignatureFormat.Rfc3279DerSequence);
        }
        catch (Exception error) when (error is FormatException or ArgumentException or CryptographicException)
        {
            return false;
        }
    }

    internal static ECDsa ReadPublicKey(string pem)
    {
        var key = ECDsa.Create();
        try
        {
            var der = PublicPem.Decode(pem, "PUBLIC KEY");
            key.ImportSubjectPublicKeyInfo(der, out var consumed);
            if (consumed != der.Length || key.ExportParameters(false).Curve.Oid.Value != "1.2.840.10045.3.1.7")
                throw new ArgumentException("invalid_public_key");
            return key;
        }
        catch (Exception error) when (error is ArgumentException or CryptographicException or FormatException)
        {
            key.Dispose();
            throw new ArgumentException("invalid_public_key");
        }
    }
}
