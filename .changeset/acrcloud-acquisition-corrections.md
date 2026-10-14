---
"waveform-one": patch
---
Correct native P4 ACRCloud acquisition, preserve bounded capture attempts through
brief quiet decisions, and honour failure backoff even when a session changes.
Reuse verified album metadata and artwork while checking actual audio for track
changes. Keep audio-presence processing independent of network requests and
retry display clears when the GUI is busy.

These corrections require a P4 firmware update. The acoustic silence classifier
is unchanged in the latest acquisition correction; recognition speed and
continuous-play/stop stability still require controlled hardware acceptance.
