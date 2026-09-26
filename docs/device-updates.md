# Signed device updates

This branch implements application updates for ARM64 Raspberry Pi OS (Python
3.13) and the ESP32-S3 16 MB controller. It does not upgrade Raspberry Pi OS,
bootloaders, partition tables or the stable recovery agent during normal updates.
Hardware commissioning and power-loss acceptance testing are still required.

## How owners receive updates

The Pi checks public GitHub releases over HTTPS monthly,
with a random delay to spread customer device traffic. No inbound ports, SSH account,
GitHub token, phone app or separate ESP Wi-Fi setup is required. The touchscreen
and paired phone show **Update available**. Settings → Updates shows release
notes and **Install update**; closing Settings defers installation.

Installation pauses recognition and the display's USB connection. The screen may
briefly show the reconnecting/update message. Keep power connected. When the
new services and firmware pass health checks, normal operation resumes. Preferences,
pairing tokens and microphone/LED wiring are preserved. No recording is sent to
the update publisher. When offline, the existing application continues working;
checks report an error and retry at the next scheduled check or manual request.

Customer devices use explicit installation. The development Pi instead uses
`development` mode: an outbound agent checks for newly signed main releases every
minute and installs them automatically after CI has published them. There is no
Install-button requirement on this device. This is a pull-based deployment agent,
not inbound SSH or a GitHub runner on the home network. If it was offline when a
release happened, it catches up after restarting. A failed version is not retried
automatically; manually check/install to retry, or publish a newer fixed release.

Only CI-published, signed `refs/heads/main` releases qualify. A failed build, a
feature-branch push, or a docs-only main push that publishes no release does not
install anything. Beta cohorts, fleet telemetry and signing-key rotation are
follow-up work, not enabled features.

## Trust and publication

`CI and Release` tests every component on main, builds the A/B firmware, builds the
ARM64 Pi application and downloads its pinned Python wheels. The Pi archive includes
these wheels for offline installation into a release-local virtual environment.
The workflow refuses to publish an update without `UPDATE_SIGNING_KEY`.

`update.json` identifies hardware, protocol, Python version, release sequence,
version, commit, ESP build identity and exact hashes/sizes of the Pi archive, ESP
application and USB provisioning bundle. `update.sig` signs the exact JSON bytes
using RSA/SHA-256. The private key stays on the publisher workstation and in
GitHub Actions secrets; devices receive only the pinned public key. The release
sequence uses this workflow's run number: preserve its monotonically increasing
history when migrating release infrastructure. Published versions are immutable.

Run on the publisher workstation once:

```sh
sh scripts/setup-update-signing.sh
```

This creates `~/.config/waveform-one-publisher/update-signing.pem`, installs it as
GitHub secret `UPDATE_SIGNING_KEY`, and exports `pi/updater/release-public.pem`.
Back up the private key securely offline. Commit only the public key. Do not run
this on customer devices, paste the private key into chat, or replace the key
without a device trust migration. Existing key mismatches fail closed.

The Pi is the trusted update coordinator. It authenticates the download before
sending it over the internal USB link. The ESP checks the received image hash and
ESP image validity; this prototype does not enable eFuse secure boot or protect
against a physically compromised Pi/USB connection.

## One-time provisioning

Do this only after resolving unstable power/connections. The current factory-only
ESP layout cannot accept A/B application updates. The provisioning bundle changes
the bootloader and partition table once, so retain a verified backup/recovery image
and do not interrupt power. Future updates write only an inactive application slot.

On the Pi, use a trusted checkout with the publisher's pinned public key:

```sh
python3 pi/updater/prepare.py pi/updater/release-public.pem "$HOME/waveform-first-update"
```

Preparation authenticates the release and downloads the signed USB bundle and
complete Pi application. It creates the application virtual environment but does
not stop services or flash anything. The destination must not already exist.

Pause music and stop the serial consumers before the separate USB flash operation:

```sh
systemctl --user stop waveform-recognition waveform-display
```

Extract the verified `waveform-one-esp32s3-usb.zip` into its own directory. With the
ESP-IDF/esptool environment available, run the `INSTALL.txt` command from that
directory, specifying the actual `/dev/serial/by-id/...` path. Do not use the old
`0x10000` app offset: the supplied `flash_args` defines the new layout. Confirm a
healthy boot and retain the recovery files. This operation is intentionally not
part of unattended updating.

Then enable updating with the real serial path (no device-specific MAC is in code):

```sh
sh pi/updater/install.sh pi/updater/release-public.pem \
  /dev/serial/by-id/YOUR_ESP_DEVICE \
  "$HOME/waveform-first-update/application"
```

For Simon's development Pi, append `development` to the installer command:

```sh
sh pi/updater/install.sh pi/updater/release-public.pem \
  /dev/serial/by-id/YOUR_ESP_DEVICE \
  "$HOME/waveform-first-update/application" development
```

Without that argument the installer selects monthly production checks and manual
installation. The mode is a local commissioning setting, not a phone control.

The installer checks the ESP build identity/health, preserves existing service
configuration, creates update overrides and installs check/install/recovery units.
It refuses to overwrite an already commissioned `current` release. Existing
`waveform-display` and `waveform-recognition` services must already be installed.
The separate recovery agent is deliberately pinned at commissioning so a broken
application release cannot replace the code needed to restore it.

## Recovery behaviour

- Signature, hardware, protocol, size and version checks precede installation.
- Downloads and Python dependency preparation finish before stopping services.
- A durable transaction records the previous Pi release before ESP writes begin.
- ESP uses 3 MB `ota_0`/`ota_1` slots and checked, sequential USB packets. Interrupted
  transfers cannot replace the running image. Inactive transfers time out.
- New firmware must initialise its audio/display pipeline and be confirmed by the
  Pi. An unconfirmed boot restarts after two minutes so the bootloader rolls back.
- A failed Pi health check restores its previous release. Recovery also rolls back
  newly booted ESP firmware or cancels a staged image that never booted.
- The installed sequence is committed only after both components pass checks.
- Recovery runs after failed updater jobs and on Pi boot. If the ESP is unplugged,
  the previous Pi app is restored and the journal is kept for another recovery try.
- Stale staging directories are removed on later checks; the current and previous
  Pi release are retained. At least 2 GB free space is required before installing.

Check state without revealing pairing tokens:

```sh
cat "$HOME/.config/waveform-one/update-status.json"
systemctl --user status waveform-update-check.timer
journalctl --user -u waveform-update-install -u waveform-update-recover
```

## Verification and remaining commissioning work

Host coverage includes signature tampering, incompatible/old manifests, corrupt
assets, archive traversal/links, USB fragmentation and offsets, incomplete/invalid
ESP images, interrupted transactions, failed Pi health, missing ESP and a crash
immediately after commit. Browser checks cover 800×480, portrait and landscape.
Firmware must build with rollback enabled and the custom partition table.

Before distributing devices: test two successive signed releases on a stable Pi/ESP,
retained settings, offline checks, interrupted transfer, bad-boot rollback, Pi restart
at transaction boundaries, and full USB recovery. Automated fakes do not establish
these hardware guarantees. Monitor and log reset reasons to resolve this prototype's
reported instability before running these tests.
