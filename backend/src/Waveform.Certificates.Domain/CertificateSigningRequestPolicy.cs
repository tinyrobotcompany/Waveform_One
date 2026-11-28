using System.Formats.Asn1;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

namespace Waveform.Certificates.Domain;

public static class CertificateSigningRequestPolicy
{
    public static bool IsValid(string pem, string hostname)
    {
        if (string.IsNullOrEmpty(pem) || pem.Length > 4096 || string.IsNullOrEmpty(hostname)) return false;
        try
        {
            var der = PublicPem.Decode(pem, "CERTIFICATE REQUEST");
            // Loading extensions is required to inspect and reject them. Signature verification
            // stays enabled; no extension is trusted or passed to an issuer before these checks.
            var request = CertificateRequest.LoadSigningRequest(der, HashAlgorithmName.SHA256,
                out var consumed, CertificateRequestLoadOptions.UnsafeLoadCertificateExtensions);
            if (consumed != der.Length) return false;
            using var key = request.PublicKey.GetECDsaPublicKey();
            if (key is null || key.ExportParameters(false).Curve.Oid.Value != "1.2.840.10045.3.1.7" ||
                !HasOnlyCommonName(request.SubjectName.RawData, hostname) || request.CertificateExtensions.Count != 1)
                return false;
            var extension = request.CertificateExtensions[0];
            return extension.Oid?.Value == "2.5.29.17" && HasOnlyDnsName(extension.RawData, hostname);
        }
        catch (Exception error) when (error is ArgumentException or CryptographicException or AsnContentException or FormatException)
        {
            return false;
        }
    }

    private static bool HasOnlyCommonName(byte[] encoded, string hostname)
    {
        var reader = new AsnReader(encoded, AsnEncodingRules.DER);
        var sequence = reader.ReadSequence();
        var set = sequence.ReadSetOf();
        var attribute = set.ReadSequence();
        if (attribute.ReadObjectIdentifier() != "2.5.4.3") return false;
        var tag = attribute.PeekTag();
        if (tag.TagClass != TagClass.Universal || tag.TagValue is not (12 or 19 or 20 or 28 or 30)) return false;
        var name = attribute.ReadCharacterString((UniversalTagNumber)tag.TagValue);
        return name == hostname && !attribute.HasData && !set.HasData && !sequence.HasData && !reader.HasData;
    }

    private static bool HasOnlyDnsName(byte[] encoded, string hostname)
    {
        var reader = new AsnReader(encoded, AsnEncodingRules.DER);
        var names = reader.ReadSequence();
        var name = names.ReadCharacterString(UniversalTagNumber.IA5String, new Asn1Tag(TagClass.ContextSpecific, 2));
        return name == hostname && !names.HasData && !reader.HasData;
    }
}
