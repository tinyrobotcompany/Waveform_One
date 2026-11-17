---
"waveform-one": patch
---

Fix strict C11 Linux compilation of the OTA worker host test. Make manual update
checks bypass scheduled deadlines even when settings changes are queued together,
while retaining provider rate limits. Give combined Pi/S3/P4 release verification
an explicit asset contract and reject invalid staging before modifying drafts.
