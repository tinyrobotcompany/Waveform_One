---
"waveform-one": patch
---

Transfer microphone audio using bounded whole-packet USB writes instead of the
character-at-a-time console path. Reject incomplete clips without disconnecting
the visualizer, retain recognition retry backoff across reconnects, and report
packet progress on capture failures without logging audio payloads.
