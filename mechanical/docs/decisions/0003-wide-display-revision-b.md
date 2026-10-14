# 0003 — Revision B: wide display concept

Date: 2026-10-10
Status: Accepted for the first full enclosure print; supersedes the stacked
layout and envelope in 0002

## Context

The owner adopted a new concept: a wide, low cabinet with rounded edges, the
7-inch screen and the LED panel side by side behind one black front, and a
holder on top for the now-playing LP sleeve. The concept shows a walnut
cabinet; for now the cabinet is 3D printed.

Hardware as of 2026-10-10 (owner):

| Item | Value |
|---|---|
| Screen | Waveshare 7-inch, HDMI video + USB touch, outline 165 × 100 mm |
| LED panel | Waveshare P3 64×32, outline 195 × 95 mm |
| Pi, ESP32, microphone | unchanged (Pi 4B, ESP32-S3-DevKitC-1, INMP441) |
| Physical knobs | still deferred |

## Decisions

- **Case size is derived**, not set: walls + side bands + screen + divider +
  LED panel across; walls + bands + the taller component up. Currently
  404 × 150 × 132 mm.
- **Shell and front plate are one piece**, split into two halves down the
  middle of the divider (where the concept already shows a line). This avoids
  a separate bezel needing its own fixings, and lets each half print front face
  down with no supports.
- **Halves join by tongue-and-groove and glue.** The rear panel's split is
  offset from the shell's, and both rear panel halves screw into both shell
  halves, so the joint is also mechanically tied.
- **Heat-set inserts everywhere a screw goes**, sized from a printed coupon
  before the large parts.
- **PETG** for all enclosure parts: the Pi warms a closed box past where PLA
  softens.
- **4 mm walls** (PRD minimum 3 mm): 6 mm walls looked closer to the concept
  but pushed each shell half past 22 h.
- **Vinyl holder**: two separate holders rather than one long rail (a 315 mm
  rail exceeds the print envelope), screwed on from inside the case.
- **The frame groove** on the front marks where the concept's wood frame meets
  the black front. A two-colour print (CANVAS) can change filament along it in
  the first 4 mm of the print only.

## Consequences

- Nothing is fit-tested yet. Screen depth, visible area and connector
  positions, LED depth, diffuser thickness and the ESP32 outline are estimates.
- Shell halves are long prints (~19–20 h each).
- The ESP32 has no mount yet (foam tape); a cradle can follow once the board
  and wiring are measured.
