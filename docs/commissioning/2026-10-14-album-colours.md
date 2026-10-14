# Apple Music–style album colours — 14 October 2026

Branch `feature/album-colours`. This replaces the 12 October prototype's
palette (`2026-10-12-album-colours.md`) and keeps its plumbing: the Album
colours / Classic black setting, its NVS persistence, the phone receiving the
palette from the device, and the 700 ms fade.

## Why the prototype was replaced

The prototype only tinted the screen. Its background was capped at 20 %
brightness, so a yellow cover gave dark olive. It rounded every colour to one
of twelve fixed hues, so different covers looked alike. Text colours never
changed, and black, white and grey covers fell back to the default green.

## What it does now

Modelled on Apple Music's artwork colours (`main/album_palette.h`):

1. The background is the cover's dominant colour, weighted towards its edges,
   light or dark as the cover is.
2. The title, artist, album and status text take the cover's other prominent
   colours. Greys become a clean near-white (or near-black), tinted with the
   background. The album and status lines are softened towards the background.
3. The heading, artwork border and status dot use the cover's most vivid
   colour, even a small one, such as a title or a sky. The search animation
   uses the next most vivid.
4. A soft colour field (8 × 5 cells of the cover's colours, pulled halfway to
   the background and blurred) fills the screen, like Apple Music's Now
   Playing. The P4 draws it full-screen with 4 × 4 ordered dithering, so the
   panel's RGB565 shows no bands. The phone draws the same cells on a canvas.
5. Readability is guaranteed. Every text colour is tested against every field
   cell as RGB565 shows it: 4.5:1 for text, and 3:1 for the large heading and
   the border.

Colours are compared in OKLab. The P4 keeps two 1.2 MB field buffers in
PSRAM. A new album's field is drawn under the old one, which then fades out.

## Offline validation

- Host tests (ASan/UBSan) cover:
  - Classic black is unchanged.
  - Solid covers in every hue, plus white, black and grey.
  - The edge-colour background.
  - Small vivid details become the accent.
  - 300 pseudo-random busy covers keep every text role readable on every
    cell.
  - Flat fields average correctly.
  - Ramps rise without bands.
  - The phone state JSON carries every role, the light flag and all 40 cells.
- The phone browser check passed: light palettes, all text roles, the colour
  scheme, the drawn field, and Classic black.
- The real LVGL screen code rendered 30 covers on the Mac at RGB565. The covers
  were fetched from the iTunes catalogue and kept locally, not committed.
- The ESP-IDF 6.1 P4 build passed.

## Still to check on hardware

- Redraw cost: during the cross-fade the field is two full-screen images, one
  blended. The search animation and clock redraw over the field image instead
  of a flat fill.
- How the colours look on the real panel, and light backgrounds in a dark
  room. A brightness cap for light covers may be wanted.
- Palette and field extraction run under the GUI lock once per album. On the
  Mac they take about 3 ms; the P4 is expected to take tens of milliseconds.
