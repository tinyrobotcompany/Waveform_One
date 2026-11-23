---
"waveform-one": patch
---

Improve P4 OTA installation reliability by waiting for background recognition and
artwork HTTPS connections to release their resources before downloading. Existing
requests can finish, and background networking resumes if installation fails;
saved recognition credentials and microphone settings are preserved.

Erase the inactive firmware slot incrementally and allow other tasks to run
between writes. Show verification only after a complete download, distinguish a
download timeout from an interrupted transfer, and add memory diagnostics.
Devices whose existing downloader cannot install this update may need a one-time
application-only cable flash. Physical OTA acceptance testing remains pending.
