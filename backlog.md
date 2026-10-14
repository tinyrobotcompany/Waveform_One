# Waveform One backlog

## Song recognition within five seconds

Status: Five-second recognition and album accuracy remain unaccepted. Four-second capture is running on hardware; Girls on Film was confirmed correct, and Inception matched the song/artist but showed the wrong album. User-reported speed was good on these trials, while device phases were approximately 5.8–6.6 seconds. Five-second normal recheck pauses and exact fingerprint normalisation are implemented and tested, awaiting flashing and measurement. The music-only classifier remains disabled; develop and validate it independently before integration. See [live trial and optimisation record](docs/commissioning/2026-10-12-four-second-recognition.md), [catalogue check](docs/commissioning/2026-10-12-catalogue-check.md) and [offline benchmark](docs/commissioning/2026-10-12-silent-music-benchmark.md).

- Measure from audible music starting to the correct song, artist and album appearing on the P4 and paired phone. Target: **five seconds or less**, including music classification, capture, fingerprinting and lookup.
- Keep a bounded rolling raw-audio buffer so music classification and recognition capture overlap. Start external lookups only after music is confirmed.
- Evaluate shorter recognition clips and faster fingerprint processing against catalogue-match reliability. Preserve raw audio, music/voice discrimination, capture integrity and service cooldowns.
- Resolve album metadata separately from song/artist matching: record returned and expected releases, check parsing against service responses, and handle identical recordings appearing on original albums, compilations and reissues without guessing.
- Validate first-song and stop/start recognition with vocal, jazz, classical and quiet music on the connected hardware. Record timing and incorrect/missed matches; do not treat a faster animation as faster recognition.
- Include long eight-second lookups and repeated artwork downloads in the internal SRAM/AES DMA budget; release temporary signature/JSON data before TLS and verify continued correct recognition under memory pressure.
- Complete this performance work and sustained-playback acceptance before PCB design.

## Music detection shared by the LED matrix and P4

Status: Completely rolled back to `f4e6752` on this feature branch, including classifier/model sources, activity protocol, capture transport/history, fingerprint experiments, retry/expiry timing and S3 build configuration. The classifier was introduced in `92103a6`, never merged to main. Speech/noise discrimination remains a future requirement; reintroduction requires measured music reliability and latency acceptance. See [complete rollback record](docs/commissioning/2026-10-11-complete-filter-rollback.md).

The previous S3 gate detected sound above a calibrated spectral noise floor. It
did not distinguish actual music from conversation or changing background noise.
Both displays must use one tested music-presence decision rather than independent
volume thresholds.

- Classify short rolling audio windows as music, speech, other noise or uncertain, including mixtures. Evaluate a compact on-device sound classifier; speech VAD alone is insufficient because singing and rap are music.
- Music-positive windows enable the LED visualization and P4 recognition. Speech/noise alone must not animate the LEDs, trigger lookups or show a music-search state.
- Smooth classifier confidence over time with separate entry/exit criteria. Short uncertain windows and conversation over a song must not discard the identified album; sustained confirmed non-music or real silence should end the music session.
- Preserve the existing fast silence response and account for classification time in the recognition latency budget. Keep a rolling prebuffer so classification does not throw away the beginning of the song.
- Keep raw audio for fingerprinting independent of visualization EQ, display gating and any speech/noise processing.
- Benchmark inference time, flash/RAM, task scheduling, USB throughput and LED refresh on the actual S3/P4 before choosing where inference runs. Do not assume a desktop model will meet MCU deadlines.
- Test real room recordings: silence, fan/traffic noise, conversation, TV dialogue, isolated transients, quiet jazz/classical, solo instruments, percussion, vocal music, rap, a cappella and music mixed with voices.
- Measure false music triggers, missed music, start/stop latency and album retention under interference. A synthetic sine-wave test or recognition-service match alone does not validate a music classifier.
- Run classification locally in native firmware; require explicit consent before collecting/storing private-room recordings or sending them elsewhere for training.

Reference candidates: [TensorFlow YAMNet sound classification](https://www.tensorflow.org/hub/tutorials/yamnet)
for classifier evaluation and [Espressif VADNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/vadnet/README.html)
for speech-detection context. The YAMNet prototype runs on the S3; its embedded
8 MB PSRAM has been identified and successfully used by the classifier.
The first short sample missed a later scheduling problem. After moving routine
printing out of the audio task and raising audio priority, a two-minute target
check reported zero dropped audio buffers and zero crashes. The user confirmed
voice rejection, music detection and a real-song match at about ten seconds.
Prioritize a rolling recognition prebuffer and evaluate shorter capture windows
against catalogue-match reliability, targeting the mandatory five-second
recognition time above. Broad noise/music-mixture and prolonged-playback acceptance remain open.
Initial fixture evidence is in
[the music-detection commissioning report](docs/commissioning/2026-10-10-music-detection.md).
Latest regression evidence and remaining limitations are in
[the music-stability commissioning report](docs/commissioning/2026-10-11-music-stability.md).

## Complete international text rendering

Status: In progress — broad glyph fallback added; full Unicode acceptance remains open.

- Preserve actual song, artist, album and user-name Unicode text; never replace valid unsupported characters with `?` or claim diagnostic boxes are real glyph coverage.
- Keep the existing typeface for supported text and supply matching fallback fonts for the remaining assigned characters, including rare CJK extensions.
- Add complete shaping/normalization for complex scripts and combining sequences; test right-to-left and mixed-direction metadata, Indic conjuncts and composed/decomposed equivalents.
- Define and validate emoji variation, joining, skin tones and flag sequences; standalone monochrome symbol coverage is not composite emoji support.
- Budget font storage, render caches and shaping memory against recognition and the planned OTA partition layout before PCB flash/RAM decisions.
- Validate real rendered words with native speakers; code-point coverage tests alone are insufficient. The current 77,872-glyph fallback resolves common missing characters but does not close this item.

## Album artwork colours for the native screen and phone

Status: Apple Music–style colours implemented on `feature/album-colours`. Host tests, the phone browser check, native LVGL renders of 30 covers and the P4 build pass. Physical P4/phone acceptance and flashing await the user's instructions.

- Modelled on Apple Music's artwork colours: the background is the cover's own dominant colour, favouring its edges, light or dark as the cover is. Dark text on light covers, light text on dark ones.
- Four text roles from the cover's other prominent colours: title, artist, album (softened) and status (softened). The heading, artwork border and status dot use the cover's most vivid colour, even a small one; the search animation uses the next.
- A soft colour field (8 × 5 cells of the cover's colours, pulled towards the background) fills the screen, like Apple Music's Now Playing. The P4 draws it with ordered dithering, so RGB565 shows no bands.
- Every text colour is checked against every field cell, as RGB565 shows it: 4.5:1 for text, 3:1 for the heading and border.
- The same palette and field on the P4 and the phone. Classic black keeps the original colours. Missing artwork and idle keep Classic.
- Hold the palette steady during rechecks of the same track; cross-fade over 700 ms when a new album is identified.
- On hardware, check: full-screen field redraw cost during the cross-fade and while the search animation runs; colour on the real panel; light backgrounds at night (a brightness cap may be wanted).

## LED visualization EQ in settings

Status: Planned — assess S3 frequency-band controls before PCB design.

- Add settings to adjust frequency-band gains (bass, mids and treble, or individual bands) and see the LED visualization respond immediately.
- These controls change the visualization only; Waveform One does not currently process the music sent to speakers.
- Define useful gain ranges, a master sensitivity control, reset to a calibrated default and saved preferences.
- Preview the effect across Classic, Mirrored and Waterfall styles, including quiet music and background noise.
- Keep visualization EQ separate from the raw recognition audio and music/silence detection so it cannot destabilize album recognition.
- Determine the S3 control protocol and DSP/headroom requirements, then validate on the current hardware before freezing the PCB.

## Magnetic wireless phone charging in the casing

Status: Planned — hardware/enclosure design investigation.

Integrate a magnetic wireless charging area into the casing so a compatible smartphone can dock and charge while Waveform One operates.

Acceptance criteria:

- Define the charging position, magnetic alignment and retention, with enough clearance for supported phones and cases.
- Select the wireless charging standard and required certification; document compatible phones, cases and supported charging power.
- Size the shared power supply and wiring for simultaneous charging, display, audio capture and LEDs, without resets or reduced recognition reliability.
- Validate surface temperature, charger ventilation, foreign-object protection and electromagnetic interference with the microphone, touchscreen and radios.
- Prototype the enclosure and test repeated docking, charging stability, thermal behaviour and uninterrupted music recognition before committing the mechanical design.

Open decisions: top/side mounting position, supported phone dimensions, charging module and power budget.

## Over-the-air firmware updates

Status: Planned — extend existing update work to the native P4/S3 device.

- Update the P4 display and S3 audio/LED controller over Wi-Fi, with clear progress and version information.
- Reconcile the existing updater, firmware transfer protocol and partition layout before selecting the native update approach; do not duplicate existing work.
- Verify firmware authenticity, board/version compatibility and the full download before installing.
- Preserve user settings and provide rollback/recovery after failed updates, lost connectivity or interrupted power.
- Coordinate compatible P4/S3 firmware versions and test interrupted updates on both boards.
- Define update notification, user initiation and whether optional automatic updates are supported.

## Add the recognized song to a playlist

Status: Planned — Apple Music first; other services subject to integration feasibility.

Add an “Add to playlist” button for the currently recognized song.

- Let the user connect and select a music service: prioritize Apple Music; investigate Spotify, Deezer, TIDAL, YouTube Music, Amazon Music and Qobuz.
- Validate official authorization and playlist-write support for each service before promising an integration; subscriptions, catalog coverage and provider restrictions may differ.
- Show the user's editable playlists, allow a default/favorite destination and support creating a new personal playlist where the service allows it.
- Offer popular/editorial playlists for discovery where available, clearly distinguish them from destinations the user has permission to edit, and allow a personal playlist inspired by one.
- Match the recognized song to the selected service's catalog, including the correct recording/version; show a confirmation and handle missing tracks, duplicate additions and expired authorization.
- Authorize accounts through the paired phone/web interface, with clear disconnect/revoke controls and protected token storage.

Reference: [Apple Music API](https://developer.apple.com/documentation/applemusicapi/) supports creating library playlists and adding tracks to existing ones. Other service integrations still need individual feasibility checks.

## USB charging and power management

Status: Planned — electrical design and battery feasibility.

The device should be rechargeable over USB-C and support battery operation.
Establish an operating-runtime target before sizing the battery.

- Measure actual P4/display, S3, HUB75 LED and wireless-phone-charger consumption, including peak simultaneous load.
- Define the USB-C power input and required USB Power Delivery profile, cable/adapter compatibility, protection and internal power distribution.
- Select a battery, battery management system and charger/power-path design; support safe charging while the device operates.
- Set brightness limits, idle/sleep behavior and a low-battery shutdown policy against the measured power budget.
- Define whether smartphone charging is available on battery power and how its load is prioritized.
- Display charge/battery state where applicable and validate runtime, thermal behavior, brownout recovery and charging under full load.

Open decisions: portable runtime, battery capacity/chemistry, charging power,
sleep consumption and phone-charging availability while unplugged.

## On/off control and personalized LED greetings

Status: Planned — user experience, firmware and power-control design.

- Use a physical power button with a controlled startup/shutdown sequence; define short-press, long-press and sleep/wake behavior.
- On startup, display `Hello <user>` on the HUB75 LED screen before entering the normal music visualization.
- On normal shutdown, display `Goodbye <user>` on the HUB75 LED screen before switching off the display, LEDs and other powered subsystems.
- Use the configured user name, with a friendly fallback, and support long names/readable text on the LED matrix.
- Allow time for the farewell, settings persistence and safe shutdown before an electronic power latch or equivalent releases power.
- Define a low-power wake circuit and distinguish standby from full power-off; USB charging must remain available when switched off.
- Define behavior after USB insertion, low battery and unexpected power loss. A farewell cannot be guaranteed when the supply is removed abruptly without stored energy.
- Test repeated on/off cycles, startup without network access and transitions while an update or recognition request is active.
