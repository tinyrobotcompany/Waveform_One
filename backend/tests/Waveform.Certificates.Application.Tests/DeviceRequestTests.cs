using System.Text;

namespace Waveform.Certificates.Application.Tests;

public sealed class DeviceRequestTests
{
    private const string Id = "0123456789abcdef0123456789abcdef";
    private static readonly string Nonce = new('a', 64);
    private static string Body(string action = "status", string extra = "", string time = "1800000000") =>
        $$"""{"device_id":"{{Id}}", "nonce":"{{Nonce}}", "issued_at":{{time}}, "action":"{{action}}"{{extra}}} """;

    [Fact]
    public void ParsesStatusAndPreservesAnOwnedCopyOfExactUtf8Bytes()
    {
        var bytes = Encoding.UTF8.GetBytes(Body());
        var expected = bytes.ToArray();
        var request = DeviceRequest.Parse(bytes);
        Array.Fill(bytes, (byte)'!');
        Assert.Equal(Id, request.DeviceId);
        Assert.Equal(Nonce, request.Nonce);
        Assert.Equal(1800000000, request.IssuedAt);
        Assert.Equal(DeviceAction.Status, request.Action);
        Assert.Null(request.Address);
        Assert.Null(request.Csr);
        Assert.Equal(expected, request.OriginalBody.ToArray());
    }

    [Fact]
    public void AcceptsEachActionWithOnlyItsRequiredField()
    {
        var address = DeviceRequest.Parse(Encoding.UTF8.GetBytes(Body("address", ",\"address\":\"10.0.0.1\"")));
        Assert.Equal(DeviceAction.Address, address.Action);
        Assert.Equal("10.0.0.1", address.Address);
        var certificate = DeviceRequest.Parse(Encoding.UTF8.GetBytes(Body("certificate", ",\"csr\":\"public CSR\"")));
        Assert.Equal(DeviceAction.Certificate, certificate.Action);
        Assert.Equal("public CSR", certificate.Csr);
    }

    [Theory]
    [InlineData("status", ",\"product\":\"tempo\"")]
    [InlineData("status", ",\"product\":\"waveform\"")]
    [InlineData("status", ",\"unknown\":true")]
    [InlineData("status", ",\"csr\":\"unexpected\"")]
    [InlineData("address", "")]
    [InlineData("certificate", "")]
    [InlineData("other", "")]
    [InlineData("address", ",\"address\":null")]
    [InlineData("certificate", ",\"csr\":{}")]
    public void RejectsUnknownForeignProductMissingOrWrongActionFields(string action, string extra) =>
        Reject(Body(action, extra), "invalid_fields");

    [Theory]
    [InlineData(",\"action\":\"status\"")]
    [InlineData(",\"a\\u0063tion\":\"status\"")]
    [InlineData(",\"nonce\":\"different\"")]
    public void RejectsDuplicateFieldsIncludingEscapedNames(string extra) =>
        Reject(Body(extra: extra), "duplicate_field");

    [Theory]
    [InlineData("true")]
    [InlineData("null")]
    [InlineData("1800000000.0")]
    [InlineData("18e8")]
    [InlineData("\"1800000000\"")]
    [InlineData("9223372036854775808")]
    public void IssuedAtMustBeAnIntegerWithinTheRepresentableRange(string time) =>
        Reject(Body(time: time), "invalid_time");

    [Fact]
    public void RejectsInvalidIdentifierNonceAndNonObjectRoots()
    {
        Reject(Body().Replace(Id, Id.ToUpperInvariant()), "invalid_device_id");
        Reject(Body().Replace(Nonce, "aa"), "invalid_nonce");
        Reject("[]", "invalid_request");
        Reject("null", "invalid_request");
        Reject("{", "invalid_json");
    }

    [Theory]
    [InlineData("{\"\\ud800\":1}")]
    [InlineData("{\"action\":\"\\ud800\"}")]
    public void InvalidEscapedUnicodeFailsWithRedactedProtocolError(string body) =>
        Reject(body, "invalid_json");

    [Fact]
    public void BodyLimitCountsUtf8BytesAndIncludesExactBoundary()
    {
        var body = Body();
        Assert.Equal(DeviceAction.Status, DeviceRequest.Parse(Encoding.UTF8.GetBytes(body.PadRight(6144))).Action);
        Reject(body.PadRight(6145), "request_too_large");
        Reject("", "request_too_large");
        var invalid = Encoding.UTF8.GetBytes(body); invalid[2] = 0xff;
        Assert.Throws<ProtocolValidationException>(() => DeviceRequest.Parse(invalid));
    }

    private static void Reject(string body, string code) => Assert.Equal(code,
        Assert.Throws<ProtocolValidationException>(() => DeviceRequest.Parse(Encoding.UTF8.GetBytes(body))).Code);
}
