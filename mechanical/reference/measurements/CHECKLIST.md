# Real-hardware measurement checklist (revision B)

Measure with digital calipers. Record each result in this folder (one file per
item), with date, tool, and a photo where useful. Once recorded, the matching
parameter in `cad/waveform_one/parameters.py` moves to MEASURED.

Most useful first: these decide whether the screen and LED panel fit.

## Waveshare 7-inch HDMI screen
- [x] Outline 165 × 100 mm (owner, 2026-10-10)
- [ ] Model name (e.g. "7inch HDMI LCD (C)")
- [ ] Total depth including the back PCB and connectors (`SCREEN_D`, assumed 13 mm)
- [ ] Visible area and its offset from the outline edges (`SCREEN_ACTIVE_*`, assumed 154.2 × 86 mm, centred)
- [ ] Where the HDMI and USB sockets sit, and which way they face
- [ ] Is the back edge flat where the clips press (35 mm in from each side)?

## Waveshare P3 64×32 LED panel
- [x] Outline 195 × 95 mm (owner, 2026-10-10)
- [ ] Depth of the frame, and depth including the HUB75 and power connectors (`LED_D`, rough 12 mm)
- [ ] Is the back edge flat where the clips press?

## LED diffuser (owner-supplied)
- [ ] Outline and thickness (`DIFFUSER_THICKNESS`, assumed 3 mm, panel-sized)

## Raspberry Pi 4 Model B
- [ ] Height with the fan-heatsink you choose (28 mm reserved above the board)
- [ ] Micro-HDMI-to-HDMI cable or adapter: plug length and bend

## ESP32-S3-DevKitC-1
- [ ] Board outline (assumed 63 × 25.5 mm) and height with the HUB75 cable attached

## INMP441 microphone board
- [ ] Board outline (assumed 15 × 15 mm) and which side the sound port is on

## Printer (from the calibration coupon, PETG)
- [ ] Best heat-set insert hole (3.8–4.2 mm)
- [ ] Smallest M3 clearance hole (3.0–3.4 mm)
