# Settings, OTA feedback and idle clock refinements

Requested 21 November 2026. Checked implementation items are complete locally; physical acceptance remains outstanding.
Use focused pull requests and regression tests before production changes.

## OTA activity feedback

- [x] Show immediate acknowledgement when Check now or Install now is pressed.
- [x] Animate the existing recognition wave motif while work is pending or active.
- [x] Show meaningful checking, downloading, verifying, restarting and failure states.
- [x] Keep device and phone feedback synchronized, including rejected requests and recovery.

## Settings navigation and phone guidance

- [x] Replace the crowded settings screen with a clear navigation hub.
- [x] Give display/brightness, appearance and Wi-Fi their own settings pages.
- [x] Make Microphone a prominent, clearly labelled destination with visible mute status.
- [x] Explain phone capabilities on Phone setup, including how to enable, view,
      export and clear listening history and its retention limitations.

## Appearance controls

- [x] Reproduce and fix the apparent lack of response from appearance buttons.
- [x] Clearly show the saved selection, its visual effect and any save failure.

## Idle dimming and clock

- [x] Add idle-dimming delays of 1, 2, 5 and 10 minutes, plus a custom delay.
- [x] Keep idle dimming configurable independently of the night schedule.
- [x] When the main screen dims, show **only the clock**: hide artwork, track
      information, recognition animation and other main-screen controls.
- [x] Apply clock-only presentation to both idle and scheduled night dimming
      whenever either dims the main screen.
- [x] Restore the normal main screen on wake; verify touch and resumed-music
      behaviour without changing recognition or microphone privacy settings.
- [x] Add persisted clock-appearance preferences, accessible from Display settings,
      with a visible preview. Define the supported appearance options in the PR.
- [x] Test interaction between idle timing, night scheduling, clock-only display,
      clock preferences, wake behaviour and unavailable wall-clock time.

## Pull requests

- [#37 — OTA activity feedback](https://github.com/tinyrobotcompany/Waveform_One/pull/37)
- [#38 — Settings, appearance and phone guidance](https://github.com/tinyrobotcompany/Waveform_One/pull/38)
- [#39 — Idle dimming and clock customization](https://github.com/tinyrobotcompany/Waveform_One/pull/39)

## Verification and delivery

- [x] Record failing regression tests before implementing each behaviour change.
- [x] Run applicable host, browser and firmware-build checks and the normal commit hook.
- [x] Open focused PRs: OTA feedback; settings navigation and phone guidance;
      idle dimming and customizable clock. Appearance feedback belongs with settings navigation.
- [x] Document hardware acceptance checks separately from automated verification.

Preflight: the configured pre-commit hook propagates test failures and blocks a
normal commit. Its test script does not include the separate CAD or browser
fixture suites. There is currently no measured overall unit-test coverage
percentage; font/glyph coverage files are not unit-test coverage evidence.

## Physical acceptance (not performed)

- [ ] On the P4 and phone, Check now and Install now acknowledge immediately;
      activity ends on success, rejection, disconnect and error.
- [ ] Open every Settings destination, return, pair a phone and reconnect Wi-Fi.
- [ ] Toggle appearance with and without artwork; confirm player and phone match.
- [ ] Mute and unmute via the prominent Microphone entry; confirm saved status.
- [ ] Test each idle delay, a custom delay, night overlap, touch/music wake,
      clock-only masking, every clock style, restart and OTA persistence.
- [ ] Verify that a cold boot without time displays no invented clock time.
