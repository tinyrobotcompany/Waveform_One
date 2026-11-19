# Recovery without company infrastructure

The repository is public, but public visibility alone does not grant an
open-source licence. This checkout has no root LICENSE file. Choosing a licence
for original software and hardware is an owner decision still outstanding;
third-party licences continue to apply independently. Do not advertise unrestricted
commercial redistribution until that decision and the dependency audit are complete.

## Independent USB build/recovery

The existing P4 source, partition table, ESP-IDF configuration and dependency
lock are in `firmware/esp32/p4_display`. Use the pinned toolchain/dependency
requirements in its README and `dependencies.lock`; archive those dependencies
for offline rebuilding. Current local validation used ESP-IDF 6.1. There is no
claim of bit-for-bit reproducibility across different toolchains/hosts.

Build without embedded account credentials:

```sh
WAVEFORM_DEVICE_ACRCLOUD=1 idf.py -C firmware/esp32/p4_display build
python3 tools/product_validation/evidence.py recovery-manifest firmware/esp32/p4_display/build > recovery-manifest.json
```

Inspect the build's `flasher_args.json` and generated partition table before
flashing. The manifest records each artifact's address, size and SHA-256, and
refuses a generated ACRCloud configuration with nonempty embedded fields. It
never reads or copies device NVS. Keep the build log, source SHA, lockfile,
bootloader, partition table, app, model and OTA initialization data together.
Do not combine a bootloader/partition table from a different release.

With the correct physical P4 port and owner authorization, the existing IDF
`flash` command performs USB recovery; see `docs/p4-ota.md` for the one-time A/B
bootstrap and 32-bit flash addressing requirement. Do not erase the device just
to test recovery: that can destroy Wi-Fi, pairing and recognition configuration.
Test a recovery kit on a spare board first. Firmware with the offline-startup
fix in PR #28 boots local audio without a recognition account; PR #31 adds
on-device owner-account setup. Older firmware does not automatically have these
behaviors.

## Signing and continuity boundary

Production OTA signature verification stays enabled. This change does not add
an unsigned remote-update bypass, export a company signing key or modify secure
boot/eFuses. A community build can be installed by the documented physical USB
path on hardware whose security configuration permits it. If future products
burn secure-boot/flash-encryption eFuses, the recovery and ownership design must
be reviewed before manufacturing; do not promise the same recovery path blindly.
Provider disappearance still removes cloud identification. Local visualization
requires the physical offline acceptance evidence, not merely a public repo.

## Release/licence audit inventory

Before distribution, retain the root licence decision, source for modified
components, ESP-IDF and managed-component licence/NOTICE files, TFLite/model
licence and provenance, cJSON notices, font licences and redistribution rights,
and hardware CAD/library licences. `dependencies.lock` pins versions but is not
a licence grant. Check all binaries included in the recovery kit against this
inventory. No missing licence is silently assigned MIT or another licence by
these tools. Current status: inventory requirements documented; complete legal
clearance and a physical recovery demonstration remain unverified.
