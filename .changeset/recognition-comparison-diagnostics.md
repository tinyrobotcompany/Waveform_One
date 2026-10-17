---
"waveform-one": patch
---

Distinguish ACRCloud no-result and fingerprint-generation failures in logs.
Add an opt-in, one-shot bench comparison of a failed sample at its original
16 kHz rate and with normalized amplitude. Disabled by default; diagnostic
requests alter timing and must not be used for latency acceptance. Record
unresolved recognition and same-album transition failures for handoff.
