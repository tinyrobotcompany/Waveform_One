# Waveform One over-the-air updates

Waveform One distributes signed releases through GitHub. The Raspberry Pi downloads
and verifies updates over the internet, updates its application, and transfers ESP
firmware over the existing USB cable. The ESP does not need its own Wi-Fi connection.

## Development and production

| Behaviour | Development device | Production/customer device |
| --- | --- | --- |
| Check for updates | Every hour, plus a check about 30 seconds after Pi boot | Monthly, with up to six hours of random delay |
| Installation | Automatic when a newer signed release is available | User selects **Install update** on the Pi or paired phone |
| Manual check | Available in Settings → Updates | Available in Settings → Updates |
| Source | Validated, signed releases from `main` | The same validated, signed releases |
| Failure | Recovery is attempted; automatic retries of that failed release pause | Recovery is attempted and the failure is shown |

Both modes require one-time commissioning. Selecting development mode does not run
feature-branch code: it follows published releases from `main`. The interval is a
local systemd timer policy, configured during commissioning. Normal application
updates do not replace this timer or the separate recovery agent.

## From a code change to a device update

1. Develop on a feature branch and open a pull request with a changeset describing
   the release impact. PR checks run the relevant tests/builds and Codex review.
2. Merge to `main`. The **CI and Release** workflow validates all components and
   builds the Pi ARM64 application and ESP32-S3 firmware.
3. If there are pending changesets, the workflow creates a versioned release with
   release notes. It checks artifact provenance and hashes, then signs the update
   manifest with the publisher's private key before publishing.
4. The Pi's scheduled agent discovers the release through outbound HTTPS. It
   verifies the signature using its pinned public key, checks device compatibility
   and release sequence, and verifies each downloaded file's size and hash.
5. A development device installs automatically. A production device displays
   **Update available**, release notes and an **Install update** action.

A feature-branch push never deploys to devices. A failed main build or a main push
that publishes no release does not deploy either. The device does not run `git pull`.
The hourly delay starts after a release becomes available, not when code is merged.

## What happens during installation

Downloads and the release-local Python environment are prepared first. The updater
then pauses recognition and display services to take exclusive control of USB.

ESP firmware is written to the inactive application slot, checked and booted. The
Pi switches to the prepared application release. It checks the Pi application's
build identity and ESP connection, then confirms the healthy ESP firmware.
Preferences, pairing credentials and wiring are retained. Expect a brief display
interruption, and keep power connected while installation is running.

These are Waveform One application/firmware updates. They do **not** update
Raspberry Pi OS, the ESP bootloader/partition table, or the separate recovery agent.

## Recovery and offline behaviour

The Pi retains the previous application release and records an update transaction.
If activation or health checks fail, recovery attempts to restore the previous Pi
release and roll back the new ESP image, or cancel an image that was only staged.
An unconfirmed ESP boot restarts after approximately two minutes so the bootloader
can roll back. Recovery also runs after failed update jobs and on Pi boot.

Development mode records failed releases rather than repeatedly reinstalling them.
A newer release can install automatically; a manual check/install permits a retry.
If the Pi is offline, the existing application keeps running and the next check
retries. Hardware disconnection can prevent recovery from completing; the journal
is retained for another attempt. Recovery is not a guarantee against every power
or hardware failure.

## Why this works in another user's home

The device makes outbound HTTPS requests to public GitHub Releases. No inbound
router ports, developer Mac, customer GitHub token or native iOS application are
required. The phone remote is a browser interface served by the Pi on the local
network. The publisher's private signing key is never installed on devices.

CI publishes updates; it does not directly connect to each customer's Pi. Periodic
checks allow devices to catch up after being offline without exposing their home
network to incoming deployment connections.

## One-time commissioning

Existing factory-layout ESP boards need a USB migration to the two-slot OTA layout.
Back up the working firmware/configuration, prepare and authenticate the published
release, migrate using the bundle's supplied flash offsets, verify ESP health, and
run the updater installer in `development` or `production` mode. Do not repeat this
migration for normal releases.

Use the [technical commissioning and recovery guide](../../docs/device-updates.md)
for commands, prerequisites, signing-key handling and sequence migration. Hardware
acceptance still requires successive-release and power-loss testing before customer
rollout; unit tests alone do not prove those guarantees.

## Checking a commissioned device

On the Pi:

```sh
cat ~/.config/waveform-one/update-status.json
systemctl --user list-timers waveform-update-check.timer
journalctl --user -u waveform-update-sync-main -u waveform-update-install -u waveform-update-recover
```

Use **Settings → Updates → Check** from the Pi or paired phone to check immediately.
In production, checking does not install an update; installation remains explicit.
