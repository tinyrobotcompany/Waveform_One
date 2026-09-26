# Colour plan (CANVAS four-colour)

Available filaments: black, orange, red, blue — in both PLA and PETG.
Resin printer: red and green resin.

## Principles

- The LED matrix is the only strong colour on the product; the enclosure frames it.
- Colour zones are **separate bodies** in the CAD and exported as a multi-body
  3MF, so ElegooSlicer assigns one filament per body. No painting in the slicer.
- Keep colour changes to small areas (inlays, lettering, trim). Alternating
  colours on large faces costs purge waste and hours of print time.

## Assignment

| Colour | Use | Visible? |
|---|---|---|
| Black (matte) | Main shell, front bezel, matrix pixel grid | Yes — dominant |
| Orange | One accent: thin trim line, logo, small markings | Yes — sparing |
| Blue | Electronics tray and component carriers | Internal only |
| Red | Joint keys, alignment pins, calibration coupons | Internal only |

Colour-coding the internal parts makes assembly self-explanatory and keeps
prototype pieces and coupons easy to tell apart.

## Resin

Good for small, precise, non-structural parts: microphone grille, logo/badge
inlay, later knob caps. Not for structural or warm parts (brittle, yellows with
heat/UV), and never in front of the LED matrix (tinted resin distorts colour).
