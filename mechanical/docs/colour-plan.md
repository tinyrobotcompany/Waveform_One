# Colour plan

## Colour stripes (2026-10-14)

Owner decision: a black cabinet with two coloured bands wrapping round it,
each 20 mm wide (widened from 10 mm the same day), starting 10 mm and 40 mm
behind the front face, with 10 mm of black before and between them.

The shell halves print front face down, so "distance behind the front" is
print height: the bands are layers 10–30 mm and 40–60 mm. Every layer is a
single colour. The CANVAS add-on changes filament only four times per shell
half (black → band 1 → black → band 2 → black), with little purge waste. A band
applied only to the outer skin would need two colours in every layer, and so a
filament change on every layer.

Each shell half's print job holds one object made of these parts, each on
its `colours.toml` colour (parts of the same colour share one filament):

| Part | Template slot | `colours.toml` | Owner's choice (2026-10-26) |
|---|---|---|---|
| `shell_…_body` | 1 | `body` | black `#000000` |
| `shell_…_stripe_1` | 2 | `stripe_1` | white `#FFFFFF` |
| `shell_…_stripe_2` | 3 | `stripe_2` | white `#FFFFFF` |
| `shell_right_name` | 4 | `name` | white `#FFFFFF` |

**White name.** WAVEFORM ONE (on the right shell, centred on the strip below the LED panel) is cut
1.0 mm into the front face and filled with a white inlay, so the front stays
flat. It occupies the first five layers, the only layers that mix two colours.
Five layers rather than three keep the black behind from greying the white.
Colours, not parts, take CANVAS slots: parts of the same colour share one
filament (2026-10-27). Black with white stripes and name uses two slots.

Colours are chosen in `colours.toml`, not in the slicer (2026-10-26): the
build writes each print job as an ElegooSlicer project with those colours in
its filament slots and the purge volumes between them, so it opens ready to
slice. The two bands can be the same colour. The bands are whole cross-sections, so their inside
features (pocket walls, clip bosses, the shell joint) print in the band colour
too. They're hidden once assembled. Positions and width are parameters
(`STRIPE_*` in `parameters.py`); the build checks that band edges fall on
0.2 mm layers.

## Revision B (2026-10-10)

The concept shows a walnut cabinet around a black front. For now the whole
cabinet is 3D printed (no real wood).

- **Default: all black PETG.** Simple, no extra filament.
- **Concept look, one shell print:** print the shell in a walnut-brown PETG and
  use the CANVAS add-on to paint the front plate black inside the engraved
  frame groove (since removed). Colour changes happen only in the first 4 mm of the print (the
  front plate), so purge waste stays small. Ask to have the front split into its
  own body if painting in the slicer is fiddly.
- Clips, tray and rear panel are hidden: any colour.

## Revision A (2026-09-26)

Owner decision: the stacked-layout enclosure printed entirely in black.

## Resin printer (red and green resin)

Not used for enclosure parts: tinted resin breaks the look, and resin is brittle
and yellows with heat and UV.
