# Device update implementation plan

Status: implementation checklist. Code and signing setup are implemented; hardware
commissioning and real power-loss acceptance tests remain. See device-updates.md.
Branch: `feature/device-ota-updates`.
Baseline: `8367eede422b5eaa97932019e6bd878b34f37fe8`.

## Product behaviour

Every device retrieves releases over outbound HTTPS using its owner's internet
connection. Neither inbound ports nor customer SSH access are required. The Pi
coordinates updates to its own application and the attached ESP32 over USB.
The existing touchscreen and paired phone interface show available version,
release notes, progress, restart requirements, failures and recovery state.
A native iOS app is not required.

Customer devices check monthly and use Install/Later. The development Pi follows
successful signed main releases automatically via a one-minute outbound agent.
Checking for an update must never interrupt listening or recording. Losing the
internet must leave installed visualisation and local controls working.

## Current verified baseline

- Main CI builds and tests firmware and Pi binaries, then publishes GitHub assets.
- Published hashes detect corruption but are not signatures.
- Pi display and recognition services are installed individually in the user's home.
- ESP firmware has one factory application partition and no update protocol.
- The existing USB control transport owns the serial connection; an updater must
  coordinate that ownership rather than opening the port concurrently.

## Implementation sequence

1. Define a versioned signed release manifest: product, hardware revision,
   platform/architecture, release sequence, channel, component versions, protocol
   compatibility, asset sizes and hashes. Pin a verification public key on devices;
   keep the private signing key only in protected release infrastructure. Reject
   bad signatures, wrong hardware, oversized artifacts and unauthorized downgrades.
2. Package complete, reproducible Pi application releases, including recognition
   dependencies, UI assets, launchers and version metadata. Preserve pairing,
   preferences and device identity outside versioned release directories.
3. Add a Pi updater service: bounded HTTPS downloads, jittered polling/backoff,
   durable transaction state, exclusive installation lock, staging, health checks,
   atomic activation and recovery to the previous release. Exercise interruption
   before and after each durable state transition. Application updates do not
   silently upgrade the operating system or change the hardware's bootloader.
4. Add ESP A/B application partitions, rollback-enabled bootloader, an explicit
   USB update protocol, image validation and a startup health acknowledgement.
   Update the inactive slot only. Coordinate capture pause and restart, timeouts,
   lost acknowledgements, bad images and power interruption with the Pi updater.
5. Add authenticated update status/actions to the existing display service and
   Settings UI. Preserve error visibility after reboot. Pair Pi/ESP versions using
   manifest compatibility rules and document recovery from either partial update.
6. Extend CI to test, package and sign releases. Publish immutable assets before
   advancing a signed channel pointer. Begin with a beta cohort, then stable;
   failed health checks must stop broader rollout. Never put publisher credentials
   on customer devices. Public distribution must remain readable without repo access.
7. Commission this prototype once, validate update and rollback on hardware, then
   document a repeatable factory installation and recovery procedure for other homes.

## One-time ESP migration

The existing single-slot layout needs a one-time USB provisioning operation before
A/B application updates are possible. Do not deploy a partition-table migration as
an ordinary application update. Prepare backup/recovery assets and verify stable
power and serial transport first. Do not burn secure-boot or anti-rollback eFuses
as part of prototype setup; production key provisioning needs a separate process.
Future normal application updates should preserve bootloader and partition table.

Reference checked 2026-09-26:
[ESP-IDF 6.1 ESP32-S3 OTA](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/ota.html).
ESP-IDF distinguishes safe inactive application-slot updates from potentially
unrecoverable interrupted bootloader/partition-table updates.

## Hardware stability and enclosure

A carrier PCB is the planned progression from jumper wiring: retain the Pi and
ESP module, use keyed/locking panel and microphone connectors, adequate power
routing and decoupling, and correctly designed logic-level buffering. A later
revision can integrate the ESP module directly. The PCB must follow measured
power and signal requirements, not simply reproduce unverified breadboard wiring.

The cause of the current instability is unverified. Capture reset reasons, USB
disconnects and service errors, and measure supply behaviour under panel load
before deciding whether firmware, connectors or power distribution need fixing.
Validate a stable hardware baseline before destructive migration or power-loss
update tests. Coordinate connector locations and board mounting with mechanical
work; this branch does not modify the mechanical worktree.

## Acceptance evidence

Automated tests: manifest authentication/compatibility; corrupted/truncated assets;
network outages; duplicate install requests; crash recovery; previous-release
restoration; ESP interrupted writes; invalid boot health; paired UI authorization.
Hardware tests: successful Pi+ESP update, retained user settings, offline operation,
power-loss recovery, failed-boot rollback and recovery after unplugged ESP USB.
A published release alone is not evidence that field updates work.
