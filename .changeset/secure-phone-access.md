---
"waveform-one": minor
---

Secure phone pairing with per-device HTTPS certificates, Secure cookies and strict HTTPS origin checks. This requires a one-time USB identity/partition migration; existing layouts reject it through ordinary OTA. Local recognition and boot remain independent of phone provisioning. Add a separate factory-only secure-boot/flash-and-NVS-encryption profile and signature-verified production packaging; no existing device eFuses are changed. See docs/secure-phone-access.md and docs/production-security.md for renewal and hardware acceptance requirements.

Phone startup now validates DNS before publishing the QR. Certificate-expiry shutdown retains credentials until the server stops successfully and retries failures without restarting a second server. Production CI uses an explicit safe test-version fallback.
