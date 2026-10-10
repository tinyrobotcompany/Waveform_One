---
"waveform-one": minor
---

Add native music recognition to the ESP32-P4 touchscreen: receive checked audio
clips from the existing ESP32-S3 over USB, generate fingerprints locally, retrieve
track metadata over verified HTTPS and display album artwork. Bound capture and
download sizes, preserve USB style controls during capture, discard results from
previous connections, retain artwork through temporary misses and retry service
failures with backoff. Physical P4-to-S3 acceptance remains required.
