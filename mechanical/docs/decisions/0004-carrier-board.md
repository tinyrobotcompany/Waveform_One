# 0004 — The casing follows the carrier board's KiCad placement

**Date:** 2026-10-13. **Status:** accepted.

The Raspberry Pi, its printed tray and the plug-in ESP32-S3 dev board are
replaced by one carrier board (see `electronics/carrier/SPEC.md`). It lies flat
behind the LED panel on four sloped floor posts with heat-set inserts.

Its outline, holes and every part's footprint area and height come from
`electronics/carrier/placement.json`. KiCad's build script generates that file
from `board.json` and the real footprints, so the casing never copies board
numbers by hand. The casing adds four things around it:

- floor posts under the holes,
- rear-panel openings at the rear-facing power sockets,
- keep-outs for the cables (to the P4, the HUB75 ribbon, the LED power wires and
  the microphone),
- the collision checks the model already runs.

One rear-panel boss on the bottom wall moved from x = 80 to x = 50 mm to clear
the board. The tray is gone from print job 4.
