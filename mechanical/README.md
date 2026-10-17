# Waveform One — mechanical

Code-first parametric CAD for the Waveform One enclosure, printed on an Elegoo
Centauri Carbon.

The model is Python source using [build123d](https://build123d.readthedocs.io).
Autodesk Fusion is used as a **viewer** for the generated STEP files, not as the
master copy. See [docs/decisions/0001-code-first-cad.md](docs/decisions/0001-code-first-cad.md).

## Status

**Revision B — wide display concept** ([decision 0003](docs/decisions/0003-wide-display-revision-b.md)):
screen and LED panel side by side in a rounded cabinet, with a vinyl-sleeve
holder on top. Designed, validated in code and slicer-checked. The test coupon
set the hole sizes (4.1 mm heat-set inserts, 3.2 mm M3 clearance). The left
shell is printed and the screen fits its pocket; the right shell, clips and
other small parts are not yet printed. The Raspberry Pi and its tray are gone:
the electronics are the P4 touchscreen board plus the carrier board
(`electronics/carrier`), whose KiCad placement the casing reads directly. Revision A (stacked layout, M0–M3) is
in Git history.

## Coordinate system

- **X**: left (−) / right (+), origin at the enclosure's horizontal centre
- **Y**: front (0) / rear (+), origin on the front face
- **Z**: bottom (0) / top (+), origin on the bottom
- Millimetres throughout.

## Setup (once)

```sh
brew install uv
cd mechanical/cad
uv sync
```

`uv` installs an isolated Python 3.12 for this folder only.

## Build

```sh
cd mechanical/cad
uv run pytest            # geometry, fit and collision checks
uv run python -m waveform_one.build
```

The build stops with a clear message if parameters are inconsistent, anything
leaves the case interior, collides with another part or a keep-out space, or
won't fit the printer. Outputs (regenerated each run, not committed):

- `exports/step/waveform_one_revB_layout.step` — whole assembly; open in Fusion
  (set **Preferences → General → Design → Default modeling orientation: Z up**)
- `exports/previews/waveform_one_revB_views.png` — four shaded views
- `exports/3mf/<part>_revB_PETG.3mf` and `exports/stl/…` — every printable part,
  already lying in print orientation

## The design

Case **401 × 150 × 132 mm** (W × D × H), derived from the components:

```text
 ╭──────────────────────────────────────────────────────╮
 │ ┌──────────────────┐ ┊ ┌──────────────────────────┐  │
 │ │   7" screen      │ ┊ │      LED panel           │  │
 │ │   165 × 100      │ ┊ │      192 × 96            │  │
 │ └──────────────────┘ ┊ └──────────────────────────┘  │
 │                      ┊      W A V E F O R M  O N E  │
 ╰──────────────────────┊───────────────────────────────╯
            left shell  ┊  right shell   (split down the divider)
```

- **Shell (2 halves):** rounded walls and black front plate in one piece per
  half. A tongue on the left half fits a groove in the right; the halves are
  glued. Printed **front face down**: no supports, smooth front.
- **Inside the front:** shallow pockets locate the screen and the LED panel
  (with your diffuser in front of it). Clips hold them in: five for the screen
  (the three top ones sit between the P4's 12-pin sockets and microphones, so
  the plugs fit), four for the LED panel.
- **Floor:** the Raspberry Pi sits on a screwed-in tray with room for a
  fan-heatsink. Vents in the floor beside it; feet seats underneath.
- **Rear panel (2 halves):** vents, a notch for the Pi and LED power cables,
  the microphone frame and sound hole. "N°1 · MARK I" and "tinyrobotcompany" are
  lettered 0.6 mm into the outer face of the left half, in the body colour. Its split is offset from the shell's so
  it ties the two shell halves together.
- **Top (optional):** two holders hold a 12-inch sleeve, leaning back 8°.
  They attach with magnets: two in each holder's base, over two glued into
  blind pockets under the top wall. Without the holders, the top is plain.
- No ESP32: the P4 drives the LED panel itself (owner, 2026-10-17), through
  the LED adapter board. The panel's size and depth come from Waveshare's
  drawing, so the LED clips press on its back.

## Printing

Material: **PETG** (a closed box with a Pi in it gets warm; PLA softens around
60 °C). Profile **0.20mm Standard @Elegoo CC 0.4 nozzle**. No supports or brim:
every file already lies the right way up — don't rotate anything.

Clean the plate with warm soapy water first. Times below use ElegooSlicer's
**Elegoo PETG** profile. The **Elegoo PETG PRO** profile limits flow and
roughly doubles the shell times (~37 h per half).

Each print is **one file** in `exports/print_jobs/`, with all its parts already
laid out on the bed. Open it (**File → Import**, **Load model only**), check the
profiles, slice, print.

| Print | File | Parts | Time |
|---|---|---|---|
| 1 | `1_calibration_coupon_revB_PETG.3mf` | hole-size coupon | 48 m |
| 2 | `2_shell_left_revB_PETG.3mf` | left shell half | 19 h |
| 3 | `3_shell_right_revB_PETG.3mf` | right shell half | 20 h |
| 4 | `4_small_parts_a_revB_PETG.3mf` | left rear panel, Pi tray, 5 screen clips | 4 h 31 m |
| 5 | `5_small_parts_b_revB_PETG.3mf` | right rear panel, 4 LED clips | not yet sliced |
| 6 | `6_record_holders_optional_revB_PETG.3mf` | 2 record holders (optional) | not yet sliced |

About 600 cm³ in total (~750 g of PETG). Single-part files are also in
`exports/3mf/` and `exports/stl/` if a part ever needs reprinting on its own.

**Colour stripes.** The two shell halves come split into a black body and two
coloured bands, each band 20 mm wide, starting 10 mm and 40 mm behind the
front ([colour plan](docs/colour-plan.md)). In ElegooSlicer, check that each
shell is **one object with three parts** in the object list. Parts set to
filament 1 print in the body colour; set filament slots 2 and 3 to the band
colours. The right shell has a fourth part, `shell_right_name`: WAVEFORM
ONE inlaid in the front face, on filament 4 (white). If the slicer shows the
parts on filament 1, set `…_stripe_1` to filament 2, `…_stripe_2` to 3 and
`shell_right_name` to 4. If
it asks whether to load the file as a single object with multiple parts,
answer **Yes**.

**Print the coupon first.** Front row: heat-set insert holes 3.8 → 4.2 mm;
back row: M3 screw holes 3.0 → 3.4 mm; the cut-off corner marks the smallest
end. Report the insert hole that grips best and the smallest hole an M3 screw
slides through; the model is rebuilt with those values before the big parts.

## Hardware

| Item | Qty | Where |
|---|---|---|
| M3 heat-set inserts, ~5.7 mm long | 23 | clip bosses 9, rear bosses 10, tray pads 4 |
| M3 × 8 screws | 23 | clips 9, rear panel 10, tray 4 |
| Neodymium disc magnets, 10 × 3 mm (N52) | 8 | record holders, optional: 2 per holder, 4 under the top |
| M2.5 × 6 screws | 4 | Pi onto the tray standoffs |
| Stick-on rubber feet, ≤ 12 mm | 4 | seats under the case |
| Epoxy or superglue | — | joining the shell halves; magnets into their pockets |

Fit inserts with a soldering iron (a dedicated insert tip helps).

## Assembly

1. Glue the shell halves (tongue into groove) on a flat surface; let it cure.
2. Press heat-set inserts into all bosses and pads.
3. Screen into its pocket, glass forward; screw on its five clips.
4. Diffuser then LED panel into the right pocket; screw on its four clips.
5. Pi onto the tray (M2.5), tray onto the floor pads.
6. Connect the LED panel: the P4 drives it through the LED adapter board
   (`electronics/led-adapter`), whose mount is still to come.
7. Microphone into the frame on the rear panel (dab of hot glue), facing the hole.
8. Feed the power cables through the rear notch; screw on both rear panel halves.
9. Feet underneath.
10. Optional record holders. Get the magnet polarity right:
    1. Glue the four magnets into the pockets under the top wall, from inside.
    2. Let the glue set.
    3. Place a loose magnet on top of the case over each one; it settles the
       way it attracts.
    4. Mark its upper face.
    5. Glue it into the holder's base pocket, marked face in first. The face
       that touched the case then faces the case again, so the holder pulls on.

## Layout

```text
mechanical/
├── cad/                    build123d source (pyproject.toml, tests/)
│   └── waveform_one/
│       ├── parameters.py   every dimension, with unit and trust status
│       ├── layout.py       case size and component positions
│       ├── build.py        entry point: validate → build → export
│       ├── validation.py   design rules, fit, collisions, print envelope
│       ├── geometry.py     shared shape helpers
│       ├── export.py       STEP, 3MF/STL and preview writers
│       ├── components/     electronics placeholders
│       ├── enclosure/      shell, clips, tray, rear panel, record holders
│       └── calibration/    hole-size coupon
├── docs/                   decisions and colour plan
├── reference/measurements/ real-hardware measurements and checklist
└── exports/                generated files only
```

## Parameter trust levels

Each parameter in `parameters.py` is tagged `VERIFIED`, `DATASHEET`,
`PROVISIONAL`, `MEASURED`, `CALIBRATED` or `POLICY` (a deliberate design rule).
