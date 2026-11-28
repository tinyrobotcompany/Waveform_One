# Production P4 security and factory service

This is a separate manufacturing profile, not an OTA switch for development units.
No provisioning script in this change burns eFuses or flashes a connected device.
First boot of a correctly commissioned production image does make irreversible
security changes. Do not run it on an existing development P4 as an experiment.

## Protections

`p4_display/sdkconfig.defaults.production` enables RSA-3072 Secure Boot V2, signed
bootloader/application verification, release-mode flash encryption and encrypted NVS.
The default NVS partition holds ACRCloud/Wi-Fi credentials and phone sessions. The
separate HTTPS NVS partition explicitly uses the same per-device NVS encryption
configuration. Neither store is erased when initialization fails. TLS keys are
scrubbed before freeing their runtime buffers; they necessarily exist in RAM while
serving HTTPS. This protects flash at rest, not a compromised running application.

JTAG allowance and insecure boot/download options are rejected by the build verifier.
ROM download is permanently restricted to Secure Download mode: ordinary debug,
readback, register access and RAM execution are unavailable. A write-only factory
service path is deliberately retained for certificate renewal/recovery. It does not
allow unsigned firmware to boot. Full permanent download disable is not selected
because it would prevent this service procedure.

The production partition table is at `0x10000`, with room for the signed bootloader.
Applications are at `0x20000` and `0x1000000`, model at `0x820000`, storage at
`0xc20000`, identity at `0x1800000`, and encrypted NVS keys at `0x1810000`.
The layout ID is `waveform-ab8-secure-v1`; development OTA manifests are incompatible.
This is a fresh factory layout, not an in-place conversion of customer data.
The reserved SPIFFS storage and public music model remain unencrypted: ESP-IDF
SPIFFS rejects encrypted partitions. Credentials are stored exclusively in encrypted
NVS, never this reserved filesystem.

## Build without provisioning hardware

Run `bash scripts/setup-idf.sh` to install the reviewed ESP-IDF 6.1.1 revision
`051e4c4eb20346e043cdd47bc388fde75ff8af63`. Node.js 22 and Git are required.
The setup and build wrappers reject any other SDK commit or HTTP parser hash.
SDK/tool installations default to the project-local `.artifacts` directory. Use an approved RSA-3072 secure-boot signing key held outside
Git with mode 0600. It is distinct from the existing OTA-manifest publisher key.

```sh
node scripts/build-production.mjs --output /private/operator/factory-build --signing-key /private/operator/secure-boot.pem --version 1.0.0
```

The output directory must be new. The tool checks the resulting effective security
configuration and verifies both bootloader and application signatures. It never
flashes. Local verification used only a temporary test key, not a production key.
A missing ACRCloud account remains a setup condition, not a boot dependency.

## Factory commissioning controls

Before first boot, a qualified operator must establish and record all of the following:

1. Correct board revision, 32 MiB flash, fresh-device eFuse state and the approved
   secure-boot public-key digest. Separate signing-key custody from device flash keys.
2. Per-device flash encryption with a hardware-generated key at first boot (never a
   shared firmware key). Protect the key with the standard IDF read/write eFuse locks.
   Application recovery then uses signed OTA; unrestricted plaintext USB reflashing
   is no longer available.
3. A unique NVS key partition generated with ESP-IDF's `nvs_partition_gen.py generate-key`.
   Keep it mode 0600. Generate `phone_tls.bin` with `phone-identity.py image --nvs-key
   /private/operator/device-a/keys/nvs_keys.bin` plus the normal identity/trust arguments.
   Provision this same NVS key partition; otherwise the encrypted identity cannot open.
4. The approved partition table, signed bootloader/application, model, initial OTA
   metadata, encrypted identity and NVS key partition installed before first normal
   boot. Use the generated build addresses; a standard application `flash_args` does
   not include the per-device identity/key or automatically install a secure bootloader.
5. Controlled first boot on stable power. Verify secure-boot enablement, release flash
   encryption, JTAG restrictions and Secure Download mode through security status.
   Confirm encrypted NVS, recognition, pairing and signed OTA on a sacrificial unit.

The exact irreversible eFuse/programming commands are intentionally not automated by
the build tool. Follow the pinned ESP-IDF procedure and record the device-specific
result; never infer successful hardware protection from a successful compilation.

## Certificate service after lockdown

Regenerate the NVS-encrypted identity with the original device NVS key. Write that
64 KiB NVS ciphertext only to `0x1800000` via Secure Download mode with no stub, then
reboot and run the HTTPS probe. The `phone_tls` partition is NVS encrypted and is
**not** marked for an additional flash-encryption layer. Do not externally flash-encrypt
this identity image. The `nvs_keys` partition is flash encrypted by the hardware;
never overwrite it during renewal. Securely escrow the per-device NVS keys with the
identity provisioning record. Losing that key removes this certificate service route.
The operator must protect these workstation copies; on-device flash encryption does
not protect files on the provisioning computer.

## Signed production OTA

Build a newer production image using the same approved boot trust key. The packaging
tool's `record` and `package` actions accept `--secure-boot-public-key /path/public.pem`.
They verify the embedded boot signature and emit the secure layout ID, in addition
to the existing signed OTA manifest. Supplying no boot key rejects secure artifacts;
supplying a key for a development build also fails. Never publish signing or device keys.

The existing main workflow remains the development-device channel. It cannot silently
publish production-secured builds: those require the explicit boot public key and
matching secure layout. Before selling factory-secured devices, establish a separately
approved secure publication channel using these packaging commands and test two
successive signed updates and rollback on production hardware. Do not switch the
current development fleet to that layout through an ordinary release.

## Evidence and remaining acceptance

Verified locally: effective configuration checks, signatures on the generated RSA
bootloader/application, nonoverlapping 32 MiB layout, 8 MiB application fit, encrypted
NVS image generation, missing-key/read-error fail-closed tests. Physical key provisioning,
power-loss trials, readback resistance, JTAG/ROM restrictions, encrypted USB service
and production OTA are not yet hardware-validated. The signed application has roughly
3% slot headroom; future additions must continue to pass the image-size check.

Source coverage: ESP-IDF v6.1 implementation/Kconfig and official documentation checked
23 November 2026: [Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/security/secure-boot-v2.html),
[flash encryption](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/security/flash-encryption.html),
[NVS encryption](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/api-reference/storage/nvs_encryption.html).
RSA is used rather than the P4 ECDSA mode affected by the documented hardware erratum.
Hardware claims remain acceptance requirements, not inferred guarantees.

## Build tooling and artifact compatibility

Repository orchestration uses Node.js 22. `scripts/idf.sh` and
`scripts/espsecure.sh` activate the verified SDK and invoke Espressif's supported
Python tools; the repository does not reimplement those tools. An existing SDK
checkout with a different commit or parser is rejected rather than repaired.
`IDF_PATH` and `IDF_TOOLS_PATH` can select an operator-managed installation.

Use `node scripts/package-p4-update.mjs record --build <build> --dist <dist>
--version <version>` with `GITHUB_SHA` set to the validated commit. The `verify`
and `package` actions take `--dist` and `--plan`. `verify` needs no signing secret;
`package` reads `UPDATE_SIGNING_KEY` only for publisher signing. Temporary keys
are mode 0600 under `.artifacts`, removed on success and failure. The OTA image,
manifest, detached signature, provenance, dependency lock, notes and checksum
filenames and formats remain compatible with the existing release contract.

Run `node --test .github/scripts/idf-security.test.mjs
.github/scripts/p4-delivery.test.mjs` for SDK rejection, production configuration,
image identity, partition bounds, signing and failure-cleanup tests. Host evidence
does not establish that any device has been provisioned or flashed.
