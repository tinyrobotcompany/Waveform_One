# Native P4 software updates

Waveform One's current ESP32-P4 controller follows Tempo's OTA implementation.
The reference is Tempo commit `2b80e871ef4c1dfcf44951d4b48efc55c8ada60a`
([runtime and operations](https://github.com/tinyrobotcompany/Tempo/blob/2b80e871ef4c1dfcf44951d4b48efc55c8ada60a/docs/OTA.md)).
The older [Pi updater](../pi/updater/README.md) remains for legacy devices.

## Owner experience

Settings → Software updates and the paired phone show installed/available versions,
status, download progress, Check now, Install now and Postpone. The phone also links
to release notes and configures automatic installation, the maintenance window
and the hourly testing option. Normal checks run every 720 hours plus up to one
hour of jitter; hourly testing adds up to five minutes. Check now is immediate
when connected with a usable clock, subject to server rate limits.

Automatic installation defaults to 03:00–04:00 in the device's existing local
timezone (Europe/Paris). Automatic installation offers a 60-second countdown.
Install now starts immediately, including during an automatic countdown.
Postpone cancels the countdown and persists a 24-hour delay. Settings
and check deadlines survive restart. Failed checks retry after 15 minutes, one
hour, then six hours; GitHub rate-limit deadlines take precedence. The existing
application continues when offline.

The installed version always identifies the running application. A local bootstrap
build reports `0.0.0`; it must not be presented as a successful release install.
Download/validation failures and interrupted activation/rollback are shown separately
from check status, so Check now does not hide the last installation result during
the current boot. A successful trial reports its version only after startup checks pass.

The phone polls live device state and OTA status independently and retries temporary
network/session failures. It clears stale OTA values if pairing is required. Valid
90-day pairing sessions survive same-address Wi-Fi reconnects and firmware restarts
using NVS and absolute expiry; restoration waits for a usable clock. Pairing is
confirmed only after storage commits successfully. Status polling does not renew
the lifetime or write to flash. Existing one-hour sessions retain their original
expiry; scan the device QR once to obtain a new 90-day session after upgrading.

Settings → Forget paired phones requires a second tap on the device and durably
revokes all phones, including across reboot; it also rotates the QR code. If
saving fails, existing pairing remains intact and the screen reports the failure.
At most four phones are paired; pairing a fifth replaces the oldest. Expiry,
cleared browser cookies, address changes or an explicit network change require
scanning the current QR again. Cookies remain HttpOnly and SameSite=Strict, and
Host/Origin/CSRF checks remain mandatory. Local HTTP still requires a trusted LAN.

The phone polls every second during update checks, installation and countdowns,
and every two seconds when idle, with bounded backoff during outages. Unsaved
update-setting edits are not overwritten by background polling.

## Necessary Waveform One adaptations

- Target: Waveshare P4 7B, revision-3 silicon, 32 MB flash, rather than Tempo's
  10.1-inch board. Application project identity is `waveform_p4_display`.
- Layout: `waveform-ab8-model-v1`, two 8 MB application slots. NVS stays at
  `0x9000`, the music model at `0x810000`, and storage at `0xc10000`. OTA data is
  at `0xf10000`, with the second app at `0xf20000`. Existing data offsets remain
  unchanged. Application-only OTA does not replace the model, S3 firmware, C6,
  partition table or bootloader. A future model/layout change needs an explicit
  compatibility migration, not an ordinary application release.
- The existing Waveform One RSA/SHA-256 publisher key is retained instead of
  copying Tempo's ECDSA key. `main/ota_public.pem` matches
  `pi/updater/release-public.pem`; the private key never belongs on the device.
- The existing combined validation/release workflow allocates the version before
  building. P4 assets join the same release as legacy Pi/S3 assets, avoiding two
  workflows racing to publish the repository's latest release.
- Local display timers and the initialized HTTP server establish trial-boot
  health. A separate task restarts an unaccepted trial after 120 seconds, allowing
  the bootloader to roll back. Internet, Shazam and the external S3 are not
  prerequisites for accepting local startup.
- Existing paired-session, Host/Origin and CSRF checks protect the phone actions.
  Tempo-specific PRIM credential controls are not copied. Release builds ignore
  embedded credentials and load the existing ACRCloud configuration from device NVS.
  Recognition never falls back to Shazam.

For owner-entered accounts, replacement and disabling, see [Your ACRCloud recognition account](owner-recognition-account.md). Owner settings take precedence over the bootstrap behavior below.

## ACRCloud credentials across updates

A local USB provisioning build reads the existing untracked `acrcloud.local` and
saves one versioned ACRCloud record to NVS. Repeated boots do not rewrite an
unchanged record. Public release builds contain no account credentials: they use
the provisioned record, which ordinary application OTA updates preserve.
`WAVEFORM_DEVICE_ACRCLOUD=1` allows a build without local credentials for this
purpose; it does not enable another recognition provider.

Devices flashed before this persistence support need one USB provisioning build
with their existing credentials. A release without a valid record reports
“ACRCloud configuration required”; it cannot silently switch providers. Trial
acceptance supports a genuinely unconfigured device: local audio capture,
classification and LEDs start without a key; only cloud recognition and its
catalogue workers remain off. Audio/network events retain the configuration
message instead of displaying a misleading searching state. No network or
successful identification is required to accept this local-only operating mode.

Invalid records, NVS read/init failures, partial bootstrap configuration and
failed persistence still block trial acceptance, preserving the protection
against credential loss during an update. They do not prevent local audio
startup on the running image. Storage failures never erase NVS. The local health
check establishes worker startup, not a physical microphone/LED acceptance test.
Valid embedded credentials whose durable write fails have an explicit
`Unpersisted` state and diagnostic: they can serve recognition for that boot,
but cannot authorize trial acceptance. If reading the owner override itself fails,
cloud recognition stays off even with embedded credentials, since the device
cannot safely determine which account the owner selected. Successful first provisioning moves
directly to `Ready`; absent configuration is the separate supported `Missing` state.

## Flash addressing repair

The second OTA slot starts at `0xf20000` and extends beyond 16 MB. Both the
application and bootloader must enable `CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH`.
A 32 MB flash-size setting alone does not enable CPU mapping above 16 MB.
Release recording rejects builds missing this option in either generated configuration.

Devices bootstrapped without it need a one-time USB update of the bootloader and
application. An application-only OTA cannot repair the installed bootloader.
Preserve the partition table, OTA selection data, NVS, model and storage; do not
perform a full-chip erase. Confirm the active slot before writing the application.
The characteristic failure is `flash_mmap: ... out of range for 24bit flash mapping`
followed by `Downloaded firmware failed device validation` after downloading.

## Publication

Reviewed main changes with a new changeset pass the existing validation gates.
The build receives `WAVEFORM_RELEASE_VERSION`; packaging checks its exact image
descriptor, version, size, rollback configuration and source/hash provenance.
The `production-ota` environment's release job uses `UPDATE_SIGNING_KEY` (an
environment secret or the existing repository secret) to sign and locally verify
the exact manifest bytes. Configure that environment to allow main only and
require `simonholmes001` as reviewer, matching Tempo. This is a GitHub environment
setting, not something declaring `environment:` in YAML creates. Verified and
configured on 2026-11-17; the only allowed deployment branch is `main`. The existing publisher setup script
provisions the repository secret without rotating an existing key.

P4 assets are `waveform-p4.bin`, `waveform-p4-manifest.json`,
`waveform-p4-manifest.sig`, `P4-SHA256SUMS`, `p4-dependencies.lock`,
`p4-update-build.json`, `p4-release.json`, and `p4-notes.md`. Draft retries replace
the draft's complete asset set. `scripts/verify-release-assets.py` checks an explicit
Pi/S3/P4 filename contract before any draft mutation, rejecting missing files,
extra files, directories and symlinks. Every uploaded asset is downloaded and compared
with the validated local file before publication. Published releases are immutable.

The device reads public GitHub metadata and verifies the signature, target,
layout, strictly newer semantic version, exact download URL, image length,
SHA-256 and ESP application descriptor before selecting the inactive slot.
Metadata cannot redirect; asset redirects are restricted to GitHub's asset CDNs.
Failed trial versions are durably blocked until a newer release appears.

## One-time USB bootstrap

The current factory-only layout cannot receive this OTA service by ordinary OTA.
Stop other flash/watch/monitor processes. A backup is optional and preserves
device-specific data rather than the source code held in GitHub.
Build the new rollback-enabled bootloader and partition table with the application:

```sh
# In an activated ESP-IDF 6.1 environment, from the repository/worktree root:
WAVEFORM_RELEASE_VERSION=0.0.0 idf.py -C firmware/esp32/p4_display build
# Inspect build/flash_args, then use the actual P4 USB-to-UART port:
idf.py -C firmware/esp32/p4_display -p /dev/cu.usbserial-YOUR_PORT flash
```

The normal flash target includes the model and initializes OTA data. Do not use
erase-flash, copy Tempo's partition offsets, or flash from concurrent worktrees.
Use a release version strictly below the next published version for an update
test. Local bootstrap version `0.0.0` is accepted by the version policy. Keep the
bootstrap/recovery files and public-key fingerprint with the device's record.

Confirm normal display/touch, retained Wi-Fi, phone pairing, audio recognition
and LED operation. Then check/install two successive signed releases. Test
postponement across restart, offline operation, loss of Wi-Fi during download,
power interruption during writes/activation/trial boot, and rollback without a
retry loop. Ordinary OTA preserves the existing model and user data.

## Evidence and remaining activation

Source coverage: Tempo's OTA worker, policy, host tests and publication workflow
were inspected and adapted; unrelated weather/transport components were not ported.
Host tests exercise the actual worker with IDF doubles, including redirects,
truncation, hash/signature failure, NVS failures, activation order and rollback
blocking. Packaging tests use real OpenSSL signatures and verify corrupted/stale
draft rejection. The browser fixture covers the actual embedded phone page.

These checks establish software behaviour, not electrical power-loss resilience.
GitHub environment protection, publication, physical bootstrap and two-release
hardware acceptance must be confirmed separately. No eFuses or hardware Secure
Boot settings are changed. Signing-key rotation needs an explicit trust transition.

## 2026-11-17 follow-up evidence

Verified: the published v0.7.1 image contains descriptor version `0.7.1`, project
`waveform_p4_display`, and matches its signed manifest hash and size. The reported
post-install `0.0.0` is therefore not a mislabeled v0.7.1 release binary. The exact
on-device failure remains unconfirmed without the installation/boot log; no claim
of successful hardware installation is made by the host/browser tests. Recognition
provider differences and transient LED behaviour are deferred to a separate task.

## P4 SDIO memory repair

The 2026-11-18 device capture showed `sdio_push_data_to_queue` and
`sdio_rx_get_buffer` asserting after failed allocations. The largest internal DMA
block fell from 25,088 bytes at startup to 560 bytes immediately before a crash,
while more than 12 MB of PSRAM remained free. The rebooted image was still 0.0.0.

The pinned ESP-Hosted driver originally allocated both growing stream buffers and
packet-pool payloads from internal DMA RAM. Its pools retained returned buffers.
For this P4/IDF 6.1 profile, transport payloads and stream storage now use PSRAM,
with addresses and capacities aligned to 128 bytes. The pinned P4 SDMMC driver
validates cache alignment and performs cache synchronization; its existing 512-byte
aligned bounce buffer covers short CMD53 tails. Small register/descriptor buffers
remain internal. The C6 streaming protocol is unchanged, transport queues are four
packets deep, and the retained buffer cache is disabled.

Failed stream growth preserves the old pointer/capacity. Receive allocation retries
release the bus lock and preserve the pending length/counter, rather than waiting
for another already-acknowledged interrupt. Failure to allocate a packet drops the
received stream; transmit failure uses the existing payload cleanup path. The
build patches copies of hash-pinned vendor sources and stops if those sources change.

For an application-only local repair, `idf.py -C firmware/esp32/p4_display -DPROJECT_VER=0.7.2 build` keeps
the base version without changing recognition credential selection. CI release
version injection still takes precedence. This prevents the existing published
0.7.2 from replacing the local repair. It is not evidence that a new signed release
has been installed: full OTA acceptance requires a later approved release containing
this fix. Do not overwrite OTA metadata, the bootloader or the model during this repair.

Hardware evidence: the application-only local repair was flashed to the existing
valid slot 0 (sequence 1) and flash hashing passed. It booted as 0.7.2 with ACRCloud,
the onboard microphone and direct P4 LED controller configured. A bounded test of
180 HTTP page transfers (3,347,280 bytes, three concurrent clients) completed in
6.0 seconds. The largest free internal DMA block stayed at 23,040 bytes; no SDIO
assertion or reboot occurred during that test. This is a network/memory smoke test,
not a completed signed-OTA install or power-loss qualification. Serial diagnostics
remain available for the touchscreen Check now validation.
