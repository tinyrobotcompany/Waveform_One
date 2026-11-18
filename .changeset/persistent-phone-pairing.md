---
"waveform-one": patch
---
Remember paired phones for 90 days across same-address reconnects and OTA restarts.
Add device-side Forget paired phones with durable revocation and retain cookie,
Host/Origin and CSRF protections. Pair only after saving succeeds, and refresh
phone update status every second during active updates. Existing one-hour sessions
need one QR rescan to obtain the longer lifetime.
