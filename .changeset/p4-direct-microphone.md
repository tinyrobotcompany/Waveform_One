---
"waveform-one": patch
---
Add an opt-in P4 onboard microphone source for recognition comparisons, retaining
S3 USB audio as the default and leaving S3 LED control intact. Capture uses the
board ES7210 driver independently of network lookups, with source and sample-rate
diagnostics. Acoustic quality and recognition improvements require hardware tests.
