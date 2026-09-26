# Real-hardware measurement checklist

Measure with digital calipers. Record each result in this folder (one file per
item, e.g. `pi7_display.md`), with date, tool, and a photo where useful. Once
recorded, the matching parameter moves from DATASHEET/PROVISIONAL to MEASURED.

Measure the real units; do not infer from diagonal sizes or old BOM values.

## Raspberry Pi 7-inch Touchscreen (original, DSI)
- [ ] Glass outline (W × H) and thickness
- [ ] Visible active area and its offset from the glass edges
- [ ] Metal frame outline behind the glass, and total depth including standoffs
- [ ] Mounting holes/standoffs: positions, thread size, height
- [ ] Adapter board: outline, height, DSI ribbon exit and bend space
- [ ] Is the Pi mounted on the display's standoffs? If so, the combined stack depth
- [ ] Power jumper/wires or USB power arrangement for the display

## Raspberry Pi 4 Model B
- [ ] Board outline, mounting hole positions (confirm against 58 × 49 mm pattern)
- [ ] Height including cooler/heatsink/fan (none fitted yet; 28 mm reserved above the board)
- [ ] USB-C, USB-A, Ethernet, micro-HDMI protrusions past the board edge
- [ ] Space for USB-C power plug and USB-A→ESP32 cable plug + bend

## Waveshare 64×32 HUB75 panel (P3)
- [x] Rough outline ~195 × 100 × 12 mm (owner, 2026-09-26; see hub75_panel.md)
- [ ] Caliper outline (expected 192 × 96 mm) and module frame depth
- [ ] Mounting points: positions, thread size or magnet posts
- [ ] Depth including HUB75 IDC connectors and power terminal
- [ ] HUB75 ribbon bend space; power lead exit

## LED diffuser (owner-supplied)
- [ ] Outline and thickness
- [ ] Is it a flat sheet, or a grid + sheet? Any mounting features?
- [ ] Gap between panel and diffuser it was designed for

## ESP32-S3 dev board
- [ ] Board outline, mounting holes (if any), height with headers
- [ ] USB connector position and cable plug + bend space

## INMP441 microphone board
- [ ] Board outline, hole position, sensor port position (top or bottom port)

## LED 5 V supply
- [ ] Internal module or external brick? Outline, mounting, input connector

## Cables
- [ ] Minimum comfortable bend: DSI ribbon, HUB75 ribbon, USB-C, USB-A
- [ ] Connector overmould lengths (the rigid part behind each plug)

## LP sleeve
- [ ] Width/height of a few real sleeves, thickness (single and gatefold)

## Printer (from M4 coupons, per material)
- [ ] Heat-set insert hole, M3 clearance, joint clearance, alignment-pin clearance
