---
"waveform-one": patch
---

Enable 32-bit flash mapping for P4 OTA validation and booting from the second
application slot. Reject release builds missing the application or bootloader
setting, and document the one-time USB correction for existing devices.

Prevent Shazam startup from overflowing the recognition task stack during its FFT
self-test, and match Software updates controls to the Settings color scheme.

Restore ACRCloud as the sole P4 recognition provider. Provision credentials once
into device NVS, retain them across OTA releases, and reject trial acceptance
without a valid persisted configuration instead of falling back to Shazam.
