# Native P4 software updates

Waveform One's current ESP32-P4 controller follows Tempo's OTA implementation.
The reference is Tempo commit `2b80e871ef4c1dfcf44951d4b48efc55c8ada60a`
([runtime and operations](https://github.com/tinyrobotcompany/Tempo/blob/2b80e871ef4c1dfcf44951d4b48efc55c8ada60a/docs/OTA.md)).
The older [Pi updater](../pi/updater/README.md) remains for legacy devices.

## Owner experience

Settings → Software updates and the paired phone show installed/available versions,
status, download progress, Check now, Install and Postpone. The phone also links
to release notes and configures automatic installation, the maintenance window
and the hourly testing option. Normal checks run every 720 hours plus up to one
hour of jitter; hourly testing adds up to five minutes. Check now is immediate
when connected with a usable clock, subject to server rate limits.

Automatic installation defaults to 03:00–04:00 in the device's existing local
timezone (Europe/Paris). Both manual and automatic installation offer a 60-second
countdown. Postpone cancels the countdown and persists a 24-hour delay. Settings
and check deadlines survive restart. Failed checks retry after 15 minutes, one
hour, then six hours; GitHub rate-limit deadlines take precedence. The existing
application continues when offline.

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
  local `acrcloud.local` diagnostics credentials and use the normal Shazam path.

## Publication

Reviewed main changes with a new changeset pass the existing validation gates.
The build receives `WAVEFORM_RELEASE_VERSION`; packaging checks its exact image
descriptor, version, size, rollback configuration and source/hash provenance.
The `production-ota` environment's release job uses `UPDATE_SIGNING_KEY` (an
environment secret or the existing repository secret) to sign and locally verify
the exact manifest bytes. Configure that environment to allow main only and
retain any desired reviewer protection. The existing publisher setup script
provisions the repository secret without rotating an existing key.

P4 assets are `waveform-p4.bin`, `waveform-p4-manifest.json`,
`waveform-p4-manifest.sig`, `P4-SHA256SUMS`, `p4-dependencies.lock`,
`p4-update-build.json`, `p4-release.json`, and `p4-notes.md`. Draft retries replace
the draft's complete asset set. Every uploaded asset is downloaded and compared
with the validated local file before publication. Published releases are immutable.

The device reads public GitHub metadata and verifies the signature, target,
layout, strictly newer semantic version, exact download URL, image length,
SHA-256 and ESP application descriptor before selecting the inactive slot.
Metadata cannot redirect; asset redirects are restricted to GitHub's asset CDNs.
Failed trial versions are durably blocked until a newer release appears.

## One-time USB bootstrap

The current factory-only layout cannot receive this OTA service by ordinary OTA.
Stop other flash/watch/monitor processes and preserve a recovery backup first.
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
