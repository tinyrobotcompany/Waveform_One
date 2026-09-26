---
"waveform-one": minor
---

Identify music through ShazamIO on the Pi and display matching track, artist,
album and cover art. Capture eight-second microphone clips over the existing
ESP USB connection with bounded queues, sequence/checksum validation and no
raw audio files. Hide expired matches, retry failures with backoff, and keep
recognition independent of the LED visualizer. Requires the updated ESP
firmware and installation of the Pi recognition service; no API key required.
