---
"waveform-one": patch
---

Fix P4 HUB75 DMA startup and restore responsive onboard-microphone LED bars
with complete low-frequency coverage, overlapping analysis, and a saved
per-band quiet-room reference. Preserve recognition PCM independently.

Treat ACRCloud's unable-to-fingerprint response as an unusable sample instead
of imposing a service-failure cooldown on subsequent music. Genuine service
errors and quota limits retain backoff. Hardware trials reported four-second
initial recognition and a three-second cached same-album track change;
broader reliability and stop-to-landing timing remain to validate.
