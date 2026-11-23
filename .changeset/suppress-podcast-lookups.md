---
"waveform-one": patch
---

Suppress song searches when the local classifier repeatedly detects speech without
music, including podcasts that continue beyond the previous 30-second fallback.
Confirmed speech also stops further chunks of a pending ACRCloud request. Keep
cautious lookups for uncertain audio, and retain singing/rapping as music cues.
This reduces unnecessary lookups; it is not a guarantee that speech can never be
misclassified or included in an audio sample. No recognition provider or saved
credentials change. Physical podcast-to-music acceptance testing remains required.
