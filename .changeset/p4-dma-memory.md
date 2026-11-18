---
"waveform-one": patch
---

Prevent hosted Wi-Fi allocation crashes during OTA traffic on the ESP32-P4.
Move transport payloads and streaming buffers into cache-aligned PSRAM, bound
queues and release idle packet buffers. Preserve the C6 streaming protocol and
handle allocation failures without asserting or corrupting buffer ownership.
