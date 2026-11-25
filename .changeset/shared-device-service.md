---
"waveform-one": patch
---

Add an opt-in production HTTPS enrollment and renewal client using Tempo's shared
Azure certificate service. Keep per-device keys in encrypted storage, bind requests
to the Waveform product namespace and preserve local operation during service
outages. Separate managed-identity OTA compatibility from existing manual and
development firmware. No service deployment or automatic device migration occurs.
The identity service receives public device identity, local IP, certificate requests
and operational metadata; it receives no audio, listening history or recognition
credentials. See docs/shared-device-service.md for provisioning and privacy details.
