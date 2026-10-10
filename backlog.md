# Waveform One backlog

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
