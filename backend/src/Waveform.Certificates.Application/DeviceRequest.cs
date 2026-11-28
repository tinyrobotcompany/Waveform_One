using System.Text;
using System.Text.Json;

namespace Waveform.Certificates.Application;

public sealed class ProtocolValidationException(string code) : Exception(code)
{
    public string Code { get; } = code;
}

public enum DeviceAction { Status, Address, Certificate }

public sealed class DeviceRequest
{
    private readonly byte[] originalBody;
    private static readonly UTF8Encoding StrictUtf8 = new(false, true);

    private DeviceRequest(byte[] body, string deviceId, string nonce, long issuedAt,
        DeviceAction action, string? address, string? csr)
    {
        originalBody = body;
        DeviceId = deviceId;
        Nonce = nonce;
        IssuedAt = issuedAt;
        Action = action;
        Address = address;
        Csr = csr;
    }

    // Parsing is not authentication/admission. Preserve the original bytes for signature
    // verification; no serializer may reconstruct a signed request from these properties.
    public static DeviceRequest Parse(ReadOnlySpan<byte> body)
    {
        if (body.Length is 0 or > 6144) throw new ProtocolValidationException("request_too_large");
        var owned = body.ToArray();
        try
        {
            StrictUtf8.GetCharCount(owned);
            using var document = JsonDocument.Parse(owned);
            if (document.RootElement.ValueKind != JsonValueKind.Object)
                throw new ProtocolValidationException("invalid_request");
            var fields = new Dictionary<string, JsonElement>(StringComparer.Ordinal);
            foreach (var property in document.RootElement.EnumerateObject())
                if (!fields.TryAdd(property.Name, property.Value))
                    throw new ProtocolValidationException("duplicate_field");
            var action = ReadString(fields, "action", "invalid_fields") switch
            {
                "status" => DeviceAction.Status,
                "address" => DeviceAction.Address,
                "certificate" => DeviceAction.Certificate,
                _ => throw new ProtocolValidationException("invalid_fields")
            };
            var expected = new HashSet<string>(["device_id", "nonce", "issued_at", "action"], StringComparer.Ordinal);
            if (action == DeviceAction.Address) expected.Add("address");
            if (action == DeviceAction.Certificate) expected.Add("csr");
            if (!expected.SetEquals(fields.Keys)) throw new ProtocolValidationException("invalid_fields");
            var id = ReadHex(fields, "device_id", 32, "invalid_device_id");
            var nonce = ReadHex(fields, "nonce", 64, "invalid_nonce");
            var timestamp = fields["issued_at"];
            if (timestamp.ValueKind != JsonValueKind.Number || !timestamp.TryGetInt64(out var issuedAt))
                throw new ProtocolValidationException("invalid_time");
            var address = action == DeviceAction.Address ? ReadString(fields, "address", "invalid_fields") : null;
            var csr = action == DeviceAction.Certificate ? ReadString(fields, "csr", "invalid_fields") : null;
            return new DeviceRequest(owned, id, nonce, issuedAt, action, address, csr);
        }
        // JsonDocument reports unpaired escaped UTF-16 surrogates as InvalidOperationException
        // when decoding a property/name, even when the UTF-8 transport bytes are well formed.
        catch (Exception error) when (error is JsonException or DecoderFallbackException or InvalidOperationException)
        {
            throw new ProtocolValidationException("invalid_json");
        }
    }

    private static string ReadString(Dictionary<string, JsonElement> fields, string name, string error)
    {
        if (!fields.TryGetValue(name, out var value) || value.ValueKind != JsonValueKind.String)
            throw new ProtocolValidationException(error);
        return value.GetString()!;
    }

    private static string ReadHex(Dictionary<string, JsonElement> fields, string name, int length, string error)
    {
        var value = ReadString(fields, name, error);
        if (value.Length != length || !value.All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f'))
            throw new ProtocolValidationException(error);
        return value;
    }

    public string DeviceId { get; }
    public string Nonce { get; }
    public long IssuedAt { get; }
    public DeviceAction Action { get; }
    public string? Address { get; }
    public string? Csr { get; }
    public ReadOnlySpan<byte> OriginalBody => originalBody;
}
