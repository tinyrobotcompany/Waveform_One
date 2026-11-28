# Certificate protocol foundation

This .NET 10 slice translates the request-validation and cryptographic policies
from Tempo `4eae79b34c7848be7ba871e0e82f44fc47b14fa1`. It is a library, not a hosted
certificate service. It introduces no Azure resources or provider calls.

`Waveform.Certificates.Domain` contains P-256 public identity hashing, strict DER
ECDSA/SHA-256 signature verification over caller-supplied exact bytes, CSR policy,
five-minute timestamp bounds, RFC1918 address policy and assigned-name generation.
`Waveform.Certificates.Application` parses bounded device requests, rejects
duplicate/unknown fields (including obsolete `product` dispatch), and retains an
owned copy of the original UTF-8 body. Parsing does not authenticate or admit a
request. The production code uses only .NET framework libraries.

The parser carries the reference fields `device_id`, `nonce`, `issued_at` and
`action`, plus exactly `address` or `csr` for the matching action. IDs and nonces
are lowercase hex of 32 and 64 characters; the limit is 6144 UTF-8 bytes. Integer
timestamps must fit `Int64`; freshness is independently checked before admission.
Addresses must use canonical dotted-decimal RFC1918 IPv4. Hostnames are derived
from the public-key identifier and an explicitly supplied namespace.

CSRs must have a valid signature, a P-256 public key, exactly the assigned subject
CN and exactly one extension containing only that DNS SAN. Loading requested
extensions is intentional so they can be rejected; signature validation is never
skipped. CA extensions, extra SAN kinds, foreign names and private-key PEM input
are rejected. Errors expose fixed codes, not supplied content or crypto exceptions.

## Explicitly deferred contract binding

The architecture plan still requires approval of the exact Waveform wire envelope.
This slice does **not** select a production signature header, context prefix,
route, hostname suffix, HTTP response/status mapping or default service endpoint.
`VerifySignedBytes` verifies the complete byte sequence supplied by its caller;
its tests use synthetic contexts to prove separation and whitespace preservation.
The eventual authenticator must prepend the approved Waveform context to
`DeviceRequest.OriginalBody`, look up the admitted device key, enforce timestamp
freshness and atomically consume the nonce with the accepted work. It must never
verify only a reserialized JSON document or trust a product/hostname from a client.

Replay storage, manufacturing admission, signed-envelope binding, Azure tables,
DNS/ACME, certificate issuance and hosting are not implemented. Passing these
tests does not establish cross-language interoperability or a working service.

## Validation

Use .NET SDK 10.0.400 or a compatible later feature band, as selected by
`global.json`. This follows the local Voxa layout and xUnit versions. Test packages
are pinned and lock files are committed; production projects have no external
packages. Run from `backend/`, keeping caches in this ignored directory:

```sh
export DOTNET_CLI_HOME="$PWD/.artifacts/dotnet-home"
export NUGET_PACKAGES="$PWD/.artifacts/nuget"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_GENERATE_ASPNET_CERTIFICATE=false
dotnet restore Waveform.Certificates.slnx --locked-mode
dotnet test Waveform.Certificates.slnx --no-restore --collect:"XPlat Code Coverage" \
  --results-directory .artifacts/test-results \
  -- RunConfiguration.TreatNoTestsAsError=true
```

No real device, credentials or live service is used in tests. Ephemeral keys are
generated in memory; assertions derive expected values from those keys and do not
depend on random output. Time inputs are explicit. No private keys are fixtures.

`sh scripts/test.sh` now runs these suites through `scripts/test-backend.sh`;
CI installs the SDK selected by `backend/global.json`. Locked restore, coverage
collection and failure on zero discovered tests are retained. To select a .NET 10
installation when another installation shadows it, set `DOTNET` to its executable
path, for example `DOTNET=/opt/homebrew/bin/dotnet sh scripts/test.sh` on this Mac.
An unavailable or incompatible SDK fails validation rather than skipping it.
