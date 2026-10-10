---
"waveform-one": minor
---

Add native music recognition to the ESP32-P4 touchscreen: receive checked audio
clips from the existing ESP32-S3 over USB, generate fingerprints locally, retrieve
track metadata over verified HTTPS and display album artwork. Bound capture and
download sizes, preserve USB style controls during capture, discard results from
previous connections or stopped playback, retry active music promptly, and retry
service failures with backoff. Keep long titles separate from artist and album
labels and remove technical implementation text from the home screen.
