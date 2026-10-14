---
"waveform-one": minor
---

Recognise songs within about four seconds of the music starting: the ESP32-S3
streams microphone audio continuously and the ESP32-P4 fingerprints it as it
arrives, looking up the first 3, 5 and 8 seconds of new sound. Detect sound and
silence against the room's own background and return to the standard screen
when the music stops. Look up once per song, timing the next lookup from the
song's length, within a fixed request budget. Show the release a listener
expects instead of singles and compilations, keeping the album already playing.
