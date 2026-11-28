using System.Security.Cryptography;
using System.Text;

namespace Waveform.Certificates.Domain.Tests;

public sealed class DeviceIdentityTests
{
    [Fact]
    public void IdentifierIsFirst128BitsOfSha256OfPublicSpki()
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        var expected = Convert.ToHexStringLower(SHA256.HashData(key.ExportSubjectPublicKeyInfo()))[..32];
        Assert.Equal(expected, DeviceIdentity.Identifier(key.ExportSubjectPublicKeyInfoPem()));
    }

    [Fact]
    public void IdentifierRejectsWrongCurveRsaAndPrivateKeyMaterial()
    {
        using var wrongCurve = ECDsa.Create(ECCurve.NamedCurves.nistP384);
        using var rsa = RSA.Create(2048);
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        Assert.Throws<ArgumentException>(() => DeviceIdentity.Identifier(wrongCurve.ExportSubjectPublicKeyInfoPem()));
        Assert.Throws<ArgumentException>(() => DeviceIdentity.Identifier(rsa.ExportSubjectPublicKeyInfoPem()));
        Assert.Throws<ArgumentException>(() => DeviceIdentity.Identifier(key.ExportPkcs8PrivateKeyPem()));
        Assert.Throws<ArgumentException>(() => DeviceIdentity.Identifier("not a public key"));
    }

    [Fact]
    public void DerSignatureAuthenticatesExactBytesWithoutReserializingOrNormalizing()
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        // Synthetic envelope only: this primitive does not select the production wire contract.
        var body = Encoding.UTF8.GetBytes("TEST-WAVEFORM\nPOST\n/example\n{\"nonce\": \"abc\"}");
        var signature = Sign(key, body);
        var pem = key.ExportSubjectPublicKeyInfoPem();
        Assert.True(DeviceIdentity.VerifySignedBytes(pem, body, signature));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, Encoding.UTF8.GetBytes("TEST-WAVEFORM\nPOST\n/example\n{\"nonce\":\"abc\"}"), signature));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, Encoding.UTF8.GetBytes("TEST-TEMPO\nPOST\n/example\n{\"nonce\": \"abc\"}"), signature));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, Encoding.UTF8.GetBytes("TEST-WAVEFORM\r\nPOST\n/example\n{\"nonce\": \"abc\"}"), signature));
    }

    [Fact]
    public void SignatureRejectsWrongKeyRawFormatInvalidBase64AndOversizedInput()
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        using var other = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        var body = "exact bytes"u8.ToArray();
        var pem = key.ExportSubjectPublicKeyInfoPem();
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, body, Sign(other, body)));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, body, Convert.ToBase64String(key.SignData(body, HashAlgorithmName.SHA256, DSASignatureFormat.IeeeP1363FixedFieldConcatenation))));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, body, "%%%"));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, body, new string('A', 105)));
        Assert.False(DeviceIdentity.VerifySignedBytes(pem, body, Sign(key, body) + "\n"));
    }

    private static string Sign(ECDsa key, byte[] message) => Convert.ToBase64String(
        key.SignData(message, HashAlgorithmName.SHA256, DSASignatureFormat.Rfc3279DerSequence));
}
