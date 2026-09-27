# Development Pi commissioning — 27 September 2026

Branch: `feature/device-commissioning`, based on main `8cb715655e0fc876ed00eba7577a4841d1d7a58f`.
Target: Raspberry Pi 4, ARM64 Raspberry Pi OS, Python 3.13.5; ESP32-S3 16 MB.
Release: signed v0.3.0 from main, not a locally built feature-branch image.

## Preflight

- Pi display and recognition services were active before commissioning.
- `vcgencmd get_throttled` returned `0x0`; approximately 104 GB disk space free.
- The configured replacement ESP was present over its existing USB connection.
- RSA signature and all downloaded asset hashes verified by `prepare.py`.
- Release-local Python environment installed using the signed archive's offline wheels.
- Existing Pi configuration saved privately on the Pi in
  `~/waveform-commissioning-20260927/pre-update-config.tar.gz`.
- Full ESP flash backup completed: 16,777,216 bytes at
  `~/waveform-commissioning-20260927/esp-before.bin`, with SHA-256 recorded
  in `esp-before.sha256` alongside it. Treat recovery files as private; do not commit them.

## Status

Commissioning completed on this prototype:

- The signed USB bundle wrote the bootloader, A/B partition table, initial OTA data
  and application at `0x20000`; esptool verified written data.
- ESP identity matched the signed manifest:
  `c4edb19f21395c1e00344b215e903f21d5340458efacba88f6cc7d776d8ffcc6`.
- ESP reported `valid` and `healthy` after migration.
- Pi application activated at `~/waveform-first-update-20260927/application` via
  the updater's `current` symlink. API build matched main `8cb7156`.
- Development timer enabled with `OnUnitActiveSec=1h` and `OnBootSec=30s`.
  The one-hour policy was applied directly during commissioning and is recorded
  in this branch for subsequent installations. Production remains monthly.
- Signed release discovery completed: `current`, `development`, `v0.3.0`.
- Display and recognition services active; API reported ESP connected, mirrored
  mode and live music activity. Pairing token and preferences matched the backup.

## Hardware recovery test

The first attempt found an audio capture still active after stopping Pi services:
`WFU ERR BUSY`. No image was written. Services were restored. The USB adapter now
retries only that explicit response for a bounded period (15-second retry window),
allowing a capture already accepted by the ESP to finish. Tests cover successful
handover, persistent BUSY and immediate propagation of other errors. The tested
adapter was installed in the separate stable updater during commissioning; the Pi
application and ESP firmware remain the published v0.3.0 artifacts.

The subsequent hardware test used the same verified release image:

1. Started a transfer, sent one packet, aborted it; running image remained intact.
2. Transferred the complete image to the inactive slot and rebooted.
3. Observed matching firmware identity, `pending` state and `healthy` status.
4. Requested explicit rollback and observed `valid`, `healthy` firmware afterward.
5. Restored display, recognition and hourly timer; verified live API and settings.

Logs are retained on the Pi in `~/waveform-commissioning-20260927/`:
`prepare.log`, `backup.log`, `flash.log`, `install.log`, `hardware-test.log`.
The complete local test suite and `git diff --check` also passed.

## Scope of acceptance

A single-device commissioning exercise does not establish fleet or power-loss reliability.
Two successive signed releases and destructive power-loss tests remain separate acceptance work.
