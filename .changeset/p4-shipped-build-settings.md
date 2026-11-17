---
"waveform-one": patch
---

Build the P4 firmware with its own microphone and the LED panel driver enabled by
default. These settings previously existed only in each checkout's untracked
sdkconfig, so fresh checkouts and CI release builds had no microphone and no LED
output. A local build without ACRCloud keys now stops with instructions instead of
silently using the fallback service.
