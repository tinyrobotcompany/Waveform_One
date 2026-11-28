using System.Formats.Asn1;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

namespace Waveform.Certificates.Domain.Tests;

public sealed class CertificateSigningRequestPolicyTests
{
    private const string Host = "d-0123456789abcdef0123456789abcdef.waveform.example.test";

    [Fact]
    public void AcceptsValidP256CsrWithExactlyAssignedCommonNameAndDnsSan()
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        Assert.True(CertificateSigningRequestPolicy.IsValid(Create(key), Host));
    }

    [Theory]
    [InlineData("foreign-name")]
    [InlineData("extra-subject")]
    [InlineData("extra-san")]
    [InlineData("email-san")]
    [InlineData("ca-extension")]
    [InlineData("missing-san")]
    public void RejectsCsrNamesAndExtensionsOutsideDevicePolicy(string variant)
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        Assert.False(CertificateSigningRequestPolicy.IsValid(Create(key, variant), Host));
    }

    [Fact]
    public void RejectsWrongCurveRsaTamperedSignatureMalformedAndOversizedCsr()
    {
        using var wrong = ECDsa.Create(ECCurve.NamedCurves.nistP384);
        Assert.False(CertificateSigningRequestPolicy.IsValid(Create(wrong), Host));
        using var rsa = RSA.Create(2048);
        var request = new CertificateRequest($"CN={Host}", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        var san = new SubjectAlternativeNameBuilder(); san.AddDnsName(Host);
        request.CertificateExtensions.Add(san.Build());
        Assert.False(CertificateSigningRequestPolicy.IsValid(request.CreateSigningRequestPem(), Host));
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        var pem = Create(key);
        var fields = PemEncoding.Find(pem);
        var der = Convert.FromBase64String(pem[fields.Base64Data]);
        der[^1] ^= 1;
        Assert.False(CertificateSigningRequestPolicy.IsValid(PemEncoding.WriteString("CERTIFICATE REQUEST", der), Host));
        Assert.False(CertificateSigningRequestPolicy.IsValid("not a CSR", Host));
        Assert.False(CertificateSigningRequestPolicy.IsValid(new string('x', 4097), Host));
    }

    [Fact]
    public void RejectsAdditionalPemObjectsAndAdditionalSanKinds()
    {
        using var key = ECDsa.Create(ECCurve.NamedCurves.nistP256);
        var pem = Create(key);
        Assert.False(CertificateSigningRequestPolicy.IsValid(pem + "\n" + pem, Host));
        var request = new CertificateRequest($"CN={Host}", key, HashAlgorithmName.SHA256);
        var san = new SubjectAlternativeNameBuilder(); san.AddDnsName(Host); san.AddIpAddress(System.Net.IPAddress.Parse("10.0.0.1"));
        request.CertificateExtensions.Add(san.Build());
        Assert.False(CertificateSigningRequestPolicy.IsValid(request.CreateSigningRequestPem(), Host));
    }

    private static string Create(ECDsa key, string variant = "")
    {
        var name = variant == "foreign-name" ? "d-foreign.tempo.example.test" : Host;
        var subject = $"CN={name}" + (variant == "extra-subject" ? ", O=Example" : "");
        var request = new CertificateRequest(subject, key, HashAlgorithmName.SHA256);
        if (variant != "missing-san")
        {
            var san = new SubjectAlternativeNameBuilder(); san.AddDnsName(name);
            if (variant == "extra-san") san.AddDnsName("another.example.test");
            if (variant == "email-san") san.AddEmailAddress("test@example.test");
            request.CertificateExtensions.Add(san.Build());
        }
        if (variant == "ca-extension") request.CertificateExtensions.Add(new X509BasicConstraintsExtension(true, false, 0, true));
        return request.CreateSigningRequestPem();
    }
}
