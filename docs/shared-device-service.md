# Managed HTTPS identity using the shared Tempo service

Waveform uses the existing Tempo certificate service and Azure foundation. It does
not deploy a second API, worker, registry, Key Vault or storage account. The backend
source and infrastructure remain owned by the Tempo repository. See its
[shared-service rollout guide](https://github.com/tinyrobotcompany/Tempo/blob/feature/shared-device-service/docs/SHARED_DEVICE_SERVICE.md).

## What changes for a manufactured device

The optional production enrollment profile generates two persistent P-256 keys on
the P4: one authenticates enrollment requests and one serves local HTTPS. Their PSA
usage policies forbid export; the software-backed key records are stored in the
existing encrypted `phone_tls` NVS partition. This protects flash at rest, not keys
against a compromised running firmware. Hardware secure boot and flash encryption
must already be enabled and factory NVS encryption keys must exist. Runtime code
never generates NVS encryption keys, erases identity storage or burns eFuses.

Manufacturing registers the PUBLIC enrollment key with the shared backend using
`--product waveform --serial <serial>`. Registration fixes the product and hostname
`d-<public-key-hash>.waveform.tinyrobotcompany.io`. Device requests cannot change
product, register themselves or obtain a certificate for another hostname. Private
keys remain on the device. Existing manually provisioned identities are rejected
by this profile rather than silently replaced.

Once time and Wi-Fi are available, the P4 signs requests to the shared HTTPS API,
updates its local IPv4 DNS record, submits a CSR and validates the returned public
certificate against embedded trust roots, hostname, validity and its local key.
Renewal starts halfway through the certificate lifetime. Pending work polls every
30 seconds; healthy devices check daily with jitter, on address changes or at the
renewal deadline. Failures back off up to six hours and honor longer Retry-After
responses up to one day. Enrollment yields to OTA through the existing network
coordination mechanism, with bounded response sizes and request time limits.

Phone pairing remains direct HTTPS between Safari and the P4. QR publication still
requires a valid certificate and DNS matching the device address. New certificates
restart HTTPS through its existing synchronous stop path. Missing enrollment,
network time, service availability or certificates do not prevent local boot,
visualization, clock or music recognition. ACRCloud's own connectivity and account
requirements remain unchanged.

## Build and compatibility

After activating the shared service, use its real output URL:

```sh
python3 scripts/build-production.py --output /path/to/new-output \
  --signing-key /protected/production-signing.pem --version X.Y.Z \
  --enrollment-url https://ACTUAL-SERVICE-HOST/v1/device
```

The command builds and verifies; it does not flash. Do not run the resulting factory
image on an existing development device without the separately reviewed production
commissioning procedure: first boot can permanently enable hardware security.

No URL is guessed or included by default. The enrollment overlay is opt-in and
requires production security. Ordinary development firmware and manual certificate
provisioning remain supported. The managed OTA contract is
`waveform-ab8-managed-v1`, distinct from manual production
`waveform-ab8-secure-v1` and development `waveform-ab8-https-v1`. Managed and manual
production share the physical partition offsets, but cannot cross-update through
ordinary OTA. Packaging requires the approved secure-boot public key for either
production contract. The current release workflow still publishes development
firmware; a reviewed production release channel and hardware acceptance are
required before customer publication.

## Privacy and availability

This service receives the device's public identifier and key, product, assigned
hostname, local IPv4 address, signed request timestamps/nonces and CSR; manufacturing
also supplies its serial. The public certificate and DNS record expose the device
hostname; certificate issuance is visible in public Certificate Transparency logs.
The DNS address is the private LAN address, not a port-forwarding instruction.
Azure ingress can observe the public source IP and operational request metadata.
No microphone audio, recognized tracks, listening history, Wi-Fi password or
ACRCloud credentials are sent to this identity service. Existing recognition
uploads and privacy disclosures remain applicable. HTTPS cannot protect a device
against arbitrary code already executing on it.

An outage delays DNS changes or renewal. A valid existing certificate can continue
serving local HTTPS while DNS still matches. Expired certificates disable secure
phone access until renewal recovers; core local operation continues. Reflashing or
application rollback must preserve encrypted identity records and device keys.

## Evidence and remaining activation checks

Host tests cover signed request construction, malformed/redirected/truncated
responses, OTA cancellation and lease release, scheduling/backoff, encrypted
storage prerequisites/write failures, immutable keys and OTA contract separation.
A real ESP-IDF crypto test validates CSR creation and certificate chains, hostname,
key, expiry and timezone-independent renewal. The managed production P4 profile
builds and fits its 8 MiB slot, with approximately 188 KiB remaining.

Read-only Azure inventory on 2026-11-25 found the existing staging foundation but
no deployed API/worker/Container Apps environment. Reuse that foundation through
Tempo's reviewed resource-group migration and deployment workflow; do not create
a parallel stack. Activating the previously absent compute introduces compute
costs. This change itself deploys nothing.

Before activation: review infrastructure what-if, verify product admission and
legacy Tempo compatibility, provider DNS ordering, revocation and renewal. On a
production test P4 verify Safari pairing/reconnect, DHCP changes, service outage,
concurrent recognition/HTTPS/OTA memory use, power loss, persistence and two signed
OTA installs. Staging CA certificates are intentionally rejected by customer trust
roots. No hardware or live-service acceptance is claimed by the automated tests.
