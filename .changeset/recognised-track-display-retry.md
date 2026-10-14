---
"waveform-one": patch
---
Retry recognised-track display updates when the P4 GUI is temporarily busy.
Newer updates and session changes supersede pending updates, and retries use
owned artwork and metadata. This fixes update delivery; hardware display timing
and acoustic stop detection remain separate acceptance checks.
