# HTTPS phone access and provisioning

For the optional shared-service enrollment profile, see [managed HTTPS identity](shared-device-service.md). Existing manual provisioning below remains supported.

The phone connects directly to the P4 over HTTPS on the home LAN. No Azure resource,
relay, public web server or cloud audio path is introduced. This adapts Tempo PR #23
(`8ecd2d1d50878ebd60a1e45509f09278b88f8832`) to the P4 partition layout and C++ callers.

Each device has a unique DNS hostname, P-256 private key and certificate. A hostname
under `tinyrobotcompany.io` is a DNS record, not a hosted application. DNS must resolve
that hostname to the device's reserved LAN address. Use a router reservation and
local/split DNS, or a public A record pointing to that private address where the
router permits it. DNS rebinding protection can block the latter. Do not forward
port 443 on the router. The QR is suppressed when DNS is unresolved, mixed, IPv6-only
or points elsewhere; the worker rechecks on startup and approximately every 15 seconds.

Use a publicly trusted CA certificate obtained using DNS validation for normal
customers. This can be issued and renewed from an operator workstation; no permanent
issuance service is required. The DNS account/CA credentials remain off the device.
A development CA is only for controlled testing: its root must be explicitly trusted
on the test phone. Never ask customers to bypass a certificate warning.

## Identity creation

Activate ESP-IDF v6.1. Keep the identity directory outside Git, on protected storage.
The tool refuses shared/wildcard identities, mismatched keys, untrusted chains,
certificates expiring within 24 hours and public-readable key files.

```sh
python scripts/phone-identity.py create --directory /private/operator/device-a --hostname device-a.waveform.tinyrobotcompany.io
```

Submit `device.csr` to the chosen CA and place the returned leaf-first certificate
chain at `fullchain.pem`. Supply the trusted issuer/root bundle explicitly (less
than 8 KiB, selected for this issuer rather than a system-wide CA store). This bundle
is stored in the identity partition, and firmware verifies the chain against it
independently of the provisioning-tool check. Self-signed device leaves are rejected:

```sh
python scripts/phone-identity.py validate --directory /private/operator/device-a --trust /private/operator/issuer-roots.pem
python scripts/phone-identity.py image --directory /private/operator/device-a --trust /private/operator/issuer-roots.pem
```

For an isolated test only, `dev-cert --directory …` creates a separate development
CA and a 30-day certificate; use that directory's `ca.pem` as `--trust`.
The generated `phone_tls.bin` contains a private key. Never publish it in a release,
log it, or commit it. The scripts do not contact a CA, modify DNS or flash hardware.

## Existing development P4 migration

The new development layout is `waveform-ab8-https-v1`. It preserves all existing
application, model, settings and storage offsets, adding the 64 KiB `phone_tls`
partition at `0x1720000`. The old layout is rejected by the OTA compatibility check:
this migration needs a planned USB install of the updated partition table/application
and this device's identity. Do not erase the whole flash. Preserve NVS recognition
credentials, Wi-Fi, history storage and the model. Do not reuse a Tempo flash offset.
A normal later application OTA preserves the certificate partition.

After migration, verify the identity without sending pairing tokens:

```sh
python scripts/check-phone-https.py --hostname device-a.waveform.tinyrobotcompany.io --address 192.168.1.26
```

Use `--ca /private/operator/device-a/ca.pem` only for the development CA. The probe
requires a trusted TLS handshake, matching hostname and an unauthenticated 401.
Scan the new QR to pair. HTTP-era sessions are invalidated; cookies are Secure,
HttpOnly and SameSite=Strict. Exact HTTPS Origin, Host and CSRF checks remain.
A DHCP address change no longer invalidates sessions if the hostname stays fixed.

Missing/bad/expired certificates disable phone access and show secure-setup instructions.
Phone service waits for usable network time and retries initialization in the background;
certificate validity is rechecked during maintenance and expired service is stopped.
They do not prevent local boot, Wi-Fi recognition, the visualizer or OTA checks.
Recognition network readiness is independent of QR/certificate readiness.

## Renewal and revocation

Maintain an inventory of hostname, serial number, expiry, issuer and provisioning
record. Renew well before expiry. Reuse the device key/hostname where appropriate,
replace `fullchain.pem`, validate and regenerate the identity image. Install it in
only the identity partition and reboot. On development devices this is a USB service
operation. Production devices require the encrypted factory-service procedure below.
No automatic renewal backend is implemented or required by this manual model.
If a device key is compromised, revoke its certificate, create a new unique key,
reprovision and use Settings → Forget paired phones. An expired certificate causes
browsers to refuse access; never restore HTTP as a workaround.

## Acceptance before customer rollout

Host tests exercise real CA/hostname/key validation, a real TLS handshake, DNS
mismatch cases and firmware initialization failures. Both P4 configurations are
build-tested. Still required on physical hardware: Safari trusted pairing, two phones,
reconnect/DHCP change, expiry/renewal, certificate absence, concurrent recognition
and OTA under TLS memory load, reboot persistence and power-loss recovery. No
hardware acceptance or eFuse provisioning is claimed by the implementation PR.

See [production security](production-security.md) for secure boot and encrypted NVS.

## HTTPS lifecycle safeguards

Startup registers the HTTP handlers and refreshes DNS before publishing the pairing
QR. The same DNS helper is used by maintenance, so a successful startup/retry does
not depend on a later maintenance tick to become available.

Expiry suspends phone authorization and hides the QR before attempting shutdown.
The application retains the server handle and staging certificate/key buffers until
`httpd_ssl_stop` succeeds. A failed stop is retried; it cannot trigger a second server
start or re-enable phone authorization through a Wi-Fi reconnect. A failed start
clears its output handle so later initialization can retry normally.

ESP-IDF v6.1 additionally copies the supplied PEM buffers into server-owned storage
in `esp_https_server/src/https_server.c`. Its stop operation delegates to `httpd_stop`,
which waits for `THREAD_STOPPED`. Host regression tests hold shutdown open with a
simulated active request, check buffer retention, inject a stop failure and verify
safe retry/restart. These tests establish application lifecycle behavior; they do
not replace physical-device TLS load/expiry acceptance.
