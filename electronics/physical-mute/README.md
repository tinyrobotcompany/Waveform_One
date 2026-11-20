# Physical microphone mute — P4 7B retrofit specification

Status: electrical design and firmware implementation; **not fitted or validated on hardware**. This is for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, not the older S3 carrier or HDMI screen. Production default remains off until the switch is installed and the acceptance checks pass.

## Electrical interface

Use a maintained, mechanically linked **4PDT, break-before-make** switch rated for low-level analogue signals. Disconnect BOTH onboard microphones, not just the channel selected by firmware. Keep microphone grounds connected. The physical contacts provide the privacy boundary even if firmware fails; GPIO is indication and upload cancellation, not the disconnect mechanism.

| Pole | Common | Listen contact | Mute contact |
| --- | --- | --- | --- |
| A | Both MIC1/MIC2 VDD pins, isolated from original bias feed | ADC_MICBIAS12 source | Open (optional load-side bleed resistor to AGND, chosen after bias-current measurement) |
| B | MIC1 DAT, isolated from its original ADC path | Original ADC-side signal path | Open |
| C | MIC2 DAT, isolated from its original ADC path | Original ADC-side signal path | Open |
| D | GND | P3 pin 1 / GPIO36 | Red indicator LED cathode; anode through 1 kΩ resistor to ESP3V3 |

GPIO36 has the board's 10 kΩ pull-up R14 and firmware enables its internal pull-up. Low means listening permitted. High/open means muted. GPIO36 is not assigned by the current HUB75 pin map. P3 pin 11 is GND and pin 12 is ESP3V3. Do not connect GPIO36 to microphone bias or to a 5 V LED supply.

Reference: [manufacturer schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf), MIC block on page 1: U17 ES7210, MIC1/MIC2 and the two 0 Ω signal links L5/L7. [Board documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B). Inspect the actual board revision and continuity before choosing cut/lift points; the schematic is not a verified soldering map. A purpose-built production PCB should route the isolated microphone nets to the switch directly. Never bridge regulated bias to ground through a switch contact.

## Parts and assembly

One maintained 4PDT switch; one red low-current LED and 1 kΩ resistor; insulated, strain-relieved short signal wiring; a connector for the sense/indicator supply. Choose the exact switch only after confirming enclosure cutout, contact ratings and signal resistance. Place the switch and indicator where a user can see and operate them. Label the positions `MIC ON` / `MIC OFF`. Keep analogue signal wiring away from HUB75 power/clock wiring and preserve microphone acoustic openings. The red LED is driven by the mechanical switch, not software; it indicates switch position, not an independent continuity measurement.

## Firmware

Enable `CONFIG_WAVEFORM_PHYSICAL_MUTE=y` only on the modified board. The setting depends on the local P4 microphone and excludes diagnostic PCM uploads. Initialization errors leave capture inhibited. Edge generations invalidate pre-mute audio even after an immediate unmute. Capture closes the ADC stream, erases its ring and working PCM, and resumes with a new segment. Presence/recognition stop; touchscreen and paired phone report `Microphone muted`. Uploads check permission before opening, between bounded 4096-byte writes and while reading; cancelled uploads are not retried. In-flight transport calls can take their configured timeout to return, and bytes already transmitted cannot be recalled. Existing unmodified hardware uses the default disabled configuration.

Muting also stops the sound-driven visualizer: it has no microphone signal. Display, settings, pairing and OTA remain available. Unmuting starts fresh capture; it must not restore an old recognition result as a new match.

## Required physical acceptance (all outstanding)

1. Unpowered continuity: every listen contact is connected only in MIC ON; both VDD and both DAT paths are open in MIC OFF; no bias/3V3/GND shorts. Record board revision and actual wiring photos.
2. Powered: measure both microphone VDD and ADC inputs in both positions. Check for back-powering, leakage and switching pops; use acoustic injection at each microphone separately. Verify no acoustic signal reaches the ADC while muted.
3. Boot muted and disconnected-sense tests: UI boots, reports muted, no recognition requests. Boot listening: recognition and LED response match the baseline. Repeat after OTA trial boot.
4. Toggle during capture, upload, slow network response and immediately back on. Confirm old audio/results are discarded and no automatic retry occurs. Record packet/request timing without retaining audio or credentials.
5. Verify red indication independent of firmware, paired-phone state, return to fresh recognition, and no HUB75/Wi-Fi interference. Exercise repeated switching and power loss.

Host tests and builds cannot certify these electrical or acoustic properties. Do not advertise a verified hardware mute until these checks are completed on the shipped design.
