# Pi display, phone remote and music recognition

Agreed scope for `feature/pi-display-remote`. OTA installation is the next phase.

## User experience

- Pi touchscreen and phone browser share one Pi control service and synchronized state.
- Phone access is over the local Wi-Fi network; no native iOS app is required.
- Controls: classic/mirrored/waterfall style, palette, microphone sensitivity,
  separate LED-panel and Pi-screen brightness.
- During music: album artwork, Now Playing label, track title, artist, album,
  smaller live visualization and the current style. Touch reveals full controls.
- Brief pauses fade the animation without switching screens. After approximately
  30 seconds without music, show a dim clock, Listening for music, current style,
  and a configurable greeting such as Hello Simon, what would you like to listen
  to today? Support clock, artwork or screen-off idle preferences.
- ESP disconnection has an explicit connection message and automatic reconnect
  status; do not mistake disconnection for silence.
- Keep ESP audio capture and panel rendering independent of Pi UI/network health.

## Recognition

- Use the existing ESP microphone; add bounded short PCM sample transfer over USB.
- Verify transfer does not disrupt audio processing or smooth panel refresh before
  relying on this capture path. No additional wiring is planned.
- Trial AudD recognition, with a replaceable provider interface. Use returned
  track/artist/album/artwork for both displays. Discogs is optional release
  enrichment; OpenAI is optional future conversation, not the recognition engine.
- Prefer source-provided metadata when an appropriate player integration exists.
- Recognition requests need explicit setup of runtime credentials, a usage budget,
  rate limits and backoff. GitHub review secrets are not runtime credentials.
- Show identifying/unidentified/offline states honestly. No matches must not
  interrupt LEDs or imply silence. Avoid presenting stale artwork as a new match.
- A recognized recording does not establish the exact vinyl pressing or edition.
- Send only bounded samples for recognition, explain this during setup, and avoid
  retaining recordings unnecessarily. Never send service secrets to the browser.

## Implementation sequence and acceptance

1. Persistent Rust Pi service with one serial-port owner, acknowledged settings,
   activity telemetry, automatic reconnect and shared state. Test reconnect,
   timeout, concurrent control requests and idle-state transitions.
2. Pi touchscreen and responsive phone controls, configurable name, Now Playing
   and idle/disconnected views. Confirm actual Pi display/touch capabilities
   before choosing the display integration. Existing architecture proposes Qt/QML.
3. Extend ESP controls and telemetry for the agreed settings and visualization;
   test ranges, persistence policy and rendering/audio behaviour.
4. PCM capture/USB transport, then bounded AudD integration and artwork handling.
   Test no-match, network failure, stale responses, track transitions and budgets.
5. Deploy to the Pi and physically verify touchscreen, phone synchronization,
   prolonged music playback, quiet passages, true silence and reconnection.

## Following phase: updates

Pi and phone should eventually show an available release, release notes, Update
now/Later and progress. The Pi downloads and installs updates and flashes the ESP
through USB. Installation, signature verification, rollback, health checks and
optional idle-time automatic updates belong to the subsequent OTA phase. Do not
expose a working-looking installation button before that service exists.

## First implementation milestone — 2026-09-26

Implemented and deployed the Rust service, shared responsive web interface,
acknowledged style controls, diagnostic-level preview, reconnect/status handling,
persistent name, idle clock, Pi backlight control/dimming, QR phone pairing and
Chromium kiosk autostart. The Pi already provides Chromium and Wayland, so this
phase uses the same browser UI on both surfaces rather than the earlier Qt/QML
proposal. No ESP firmware changes or audio uploads were made in this milestone.

Verified: host tests, HTTP pairing/validation/persistence integration tests,
Rust linting, ARM64 native build, actual ESP acknowledgements for all three
styles, and screenshots at the Pi's 800x480 and phone's 390x844 dimensions.
Physical touch and prolonged pause/resume behaviour still need user observation.

Remaining: richer ESP controls, PCM transfer, AudD credentials/integration,
actual album metadata/artwork, idle preferences, recognition budget/backoff and
end-to-end tests for those capabilities. OTA installation remains a later phase.
