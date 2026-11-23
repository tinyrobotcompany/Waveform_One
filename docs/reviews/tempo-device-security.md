# Tempo HTTPS and enrollment assessment — 23 November 2026

This is a review and recommendation, not authorization to deploy Azure or change
Waveform One's enrollment architecture. The associated implementation only converts
the five remaining custom P4 C modules to C++.

## Findings

### High — enrollment keys are not stored in encrypted NVS

Verified in `feature/device-enrollment` at `2c44bc2`: [device_identity.c, lines 27–83](https://github.com/tinyrobotcompany/Tempo/blob/2c44bc292feb91b4688541bc8790942cfc512f1e/firmware/main/device_identity.c#L27)
loads and writes the permanent authentication key and TLS key through ordinary
`nvs_flash_init_partition("phone_tls")`. The production guard checks secure boot
and flash encryption, but the partition table supplies no NVS key partition and
this path never calls secure NVS initialization. Physical flash extraction could
therefore recover the identity keys even with those two hardware protections active.

ESP-IDF distinguishes [NVS encryption](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/storage/nvs_encryption.html)
from [flash encryption](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/security/flash-encryption.html).
The local v6.1 `nvs_flash_init_partition()` implementation confirms it initializes
ordinary storage; enabling encryption for the default NVS partition does not
implicitly secure this separate partition.

**Before adoption:** retain Waveform One's secure initialization and protected NVS
key partition, require them before generating either identity, and test missing
keys, plaintext storage rejection, reboot and power loss. Do not replace the
current Waveform provisioning protections with Tempo's guard alone.

### High — the enrollment branch still rejects a normal CA-issued certificate

Verified in [phone_tls.c, lines 61–67](https://github.com/tinyrobotcompany/Tempo/blob/2c44bc292feb91b4688541bc8790942cfc512f1e/firmware/main/phone_tls.c#L61):
the enrollment branch validates the leaf certificate against itself as its trust
anchor. A temporary host build using the real ESP-IDF Mbed TLS libraries and
Tempo's CA-issued fixture failed the successful-start assertion at this commit.
The same test passed with local secure-phone-access commit `381a2ac`, including
negative tests for wrong hostname, wrong key and malformed certificate.

**Before adoption:** integrate the existing `381a2ac` correction into the enrollment
branch and retain the real-library regression test. This is already fixed locally
on another branch; it is not a request to invent a second fix. At review time that
commit was absent from the remote secure-phone-access branch and enrollment branch.
Waveform already has its own certificate validation implementation and should retain it.

### Medium — healthy devices poll the backend continuously

Verified in [device_enrollment.c, lines 65–104](https://github.com/tinyrobotcompany/Tempo/blob/2c44bc292feb91b4688541bc8790942cfc512f1e/firmware/main/device_enrollment.c#L65):
every successful tick requests status, then sleeps 30–44 seconds, even when the
certificate remains valid and the address has not changed. Each accepted status
request reads storage and commits a replay nonce plus updated state. The response
also carries the certificate chain.

Ignoring request duration, this is approximately 59,000–86,000 status requests per
device per 30 days, before enrollment, address changes or retries. This is a derived
traffic estimate, not a measured Azure bill. It also creates avoidable TLS work on
the P4, where recognition and OTA already compete for memory.

**Before adoption:** poll quickly only while a job is pending; otherwise use the
locally known renewal deadline, address-change events and a deliberately chosen
infrequent health check. Honor server backoff, add jitter, and make all enrollment
connections obey Waveform's existing network pause/drain coordination.

### Medium — TLS restart discards a failed-stop result

Verified in [phone_tls.c, line 94](https://github.com/tinyrobotcompany/Tempo/blob/2c44bc292feb91b4688541bc8790942cfc512f1e/firmware/main/phone_tls.c#L94)
and [phone_setup.c, lines 52–59](https://github.com/tinyrobotcompany/Tempo/blob/2c44bc292feb91b4688541bc8790942cfc512f1e/firmware/main/phone_setup.c#L52):
stop unconditionally clears identity buffers, the caller loses the server handle,
and renewal attempts to start a replacement. ESP-IDF v6.1 does synchronously drain
on successful stop, so success alone is not a lifetime defect. However, sending
the shutdown control message can fail, returning before the server stops. That
failure must not be treated as success.

**Before adoption:** retain Waveform's existing stop-result handling, ownership and
retry tests. Keep the old handle and identity alive until successful shutdown.

## Recommendation

Adopt the **architecture in principle, after the above corrections**, rather than
copying all three branches verbatim. Device-generated keys, manufacturing admission,
signed enrollment requests, automatic certificate renewal and address updates solve
a real consumer setup problem: users should not manually install certificates or
connect their device to a developer's laptop to renew them.

Azure is a reasonable host for this automation. It is not inherently required for
HTTPS. A subdomain supplies a name; the backend operates certificate issuance,
renewal and DNS. In this design Azure hosts the API and workers, Let's Encrypt
issues certificates, and Spaceship operates DNS. Phone control remains a direct
HTTPS connection over the home network; Azure is not a proxy for the phone UI.
The enrollment messages contain public identity/CSR, device identifier and private
LAN address, not recorded audio or ACRCloud credentials. Infrastructure access logs
may also contain the connection's public IP; retention must be documented.

Prefer reusing a reviewed service implementation for both products, with explicit
product namespaces, admission records and authorization boundaries. Decide whether
to share a deployment after evaluating costs and failure isolation. Implement the
Waveform client in C++, preserving its existing per-phone sessions, encrypted NVS,
OTA controls, memory coordination and independent local startup.

Additional conditions before production adoption:

- **Fleet issuance:** Let's Encrypt currently permits 50 new certificates per
  registered domain per seven days. Tempo and Waveform subdomains under
  `tinyrobotcompany.io` share that limit. Arrange capacity/overrides before onboarding
  a larger fleet; do not assume separate product subdomains reset it. The reviewed
  issuer has per-device limits, not a fleet-wide issuance budget.
  [Official limits](https://letsencrypt.org/docs/rate-limits/).
- **Operating cost:** the infrastructure keeps an API and worker replica running,
  plus registry, storage, Key Vault, private endpoints and logs. Free certificates
  do not mean free operation. Price the chosen region and expected fleet, reduce
  polling first, and set budgets and alerts. No live Azure resources or actual bill
  were inspected. [Container Apps billing](https://learn.microsoft.com/en-us/azure/container-apps/billing).
- **Home-network compatibility:** public DNS records resolving to private LAN IPs
  can be filtered by routers/resolvers. Test Safari with representative routers,
  DNS protection and DHCP changes; a successful device-side DNS check alone does
  not prove the phone can resolve the same address.
- **Continuity:** a backend outage must leave recognition, LEDs, display and controls
  usable locally. Existing phone HTTPS can work while its certificate and DNS are
  usable; indefinite service shutdown eventually prevents trusted phone access.
  Document recovery, domain ownership, key/registry backup and a supported successor
  service or firmware migration. Do not promise lifetime offline phone HTTPS.
- **Privacy and operations:** document stable public hostnames/certificate transparency,
  DNS address publication and enrollment metadata; protect DNS credentials; configure
  alerts and admission/disable/recovery procedures. Add public API abuse controls and
  test throttling before exposing a production fleet endpoint.

## Source coverage summary

Read-only assessment of the three requested Tempo branches:

| Branch | Reviewed commit / state |
| --- | --- |
| `feature/secure-phone-access` | Remote `bb2d564aa2d9f02f8ef7ef349834e681f5f71bb0`; local correction `381a2ac9f89e9a295f798ba6aabac46981681ac2` |
| `feature/device-certificate-service` | `54b70a8fce14b757a128c5dd73afcb5d97c1b719`, plus separately inspected uncommitted changes |
| `feature/device-enrollment` | `2c44bc292feb91b4688541bc8790942cfc512f1e` |

Inspected identity generation/storage, signed protocol, HTTPS startup/restart, enrollment
scheduling, API admission/replay transactions, CA/DNS workers, infrastructure roles,
networking/deployment documentation and associated tests. Service work in progress
adds bounded per-device nonce cleanup, transaction tests and bootstrap parameters;
these changes were not mistaken for committed branch contents. Unrelated Tempo UI,
weather and transport features were not reviewed.

## Evidence quality notes

- **Verified:** 35 backend tests passed from an exported committed snapshot; 39 passed
  against the local service work in progress, without modifying Tempo's source.
- **Verified:** real-library certificate regression reproduced against enrollment
  and passed with the local secure-phone-access correction.
- **Verified:** storage initialization and stop failure behavior checked against the
  installed ESP-IDF v6.1 implementation; official guidance checked on review date.
- **Derived:** traffic estimate and reuse recommendation follow from inspected code;
  neither is a deployed load test or billing measurement.

## Assumptions and residual risks

No Azure deployment, live CA issuance, DNS mutation, manufacturing enrollment,
hardware provisioning, eFuse changes, device flashing or phone acceptance test was
performed. Passing host tests does not establish production readiness. Recheck the
branch heads and outstanding findings when implementation is approved: Tempo's work
was still changing locally during this review.
