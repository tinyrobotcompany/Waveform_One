---
"waveform-one": patch
---
Drain queued microphone samples before selecting the next recognition window
after a slow lookup or artwork request. This prevents repeated requests from
following old buffered audio while newer audio is available. Request spacing,
service backoff, disconnect handling and acoustic thresholds remain unchanged.
Hardware song-change timing still requires validation.
