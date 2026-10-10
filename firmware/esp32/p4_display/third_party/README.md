# Recognition source provenance

`cJSON.c`, `cJSON.h` and `cJSON-LICENSE.txt` are upstream
[cJSON v1.7.19](https://github.com/DaveGamble/cJSON/tree/v1.7.19). Trailing whitespace is normalized; source behavior is unchanged. Device and host
tests compile this same source with `CJSON_NESTING_LIMIT=32`. Host builds suppress
the macOS SDK's deprecation warning for upstream `sprintf`; application warnings
remain errors. Vendoring keeps the existing host test setup independent of an
ESP-IDF installation or an additional JSON library installation.

The native fingerprint implementation adapts the legacy algorithm and binary
format from [ShazamIO 0.8.1, commit
b5321b5c15d88ed98663420e63916704c6537512](https://github.com/shazamio/ShazamIO/tree/b5321b5c15d88ed98663420e63916704c6537512).
Its MIT notice is preserved in `ShazamIO-LICENSE.txt`. The device uses no Python,
Rust runtime or audio codec for recognition; it processes the S3's PCM directly.

`tests/fingerprint_reference.h` records the upstream Python/NumPy float64 result
for 128,000 deterministic samples. Start a uint32 LCG at `0x12345678`, update it
with `state = 1664525 * state + 1013904223` modulo 2^32, and take each sample as
`(state >> 16) - 32768`. Feed those samples to the unmodified upstream
`SignatureGenerator.get_next_signature()`. The reference contains 288 peaks and
49,664 processed samples. Native float32 results must match every band and FFT
pass; magnitude and frequency may differ by at most one integer unit. The
separate encoded-URI fixture is byte-exact and exercises an extended pass offset.

This remains an unofficial Shazam adapter, as on the previous Pi prototype.
Successful reference recognition establishes current protocol compatibility,
not a commercial service agreement or guaranteed availability.
