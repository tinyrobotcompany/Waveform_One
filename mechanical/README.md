# Waveform One — mechanical

Code-first parametric CAD for the Waveform One enclosure, printed on an Elegoo
Centauri Carbon, entirely in black (see [docs/colour-plan.md](docs/colour-plan.md)).

The model is Python source using [build123d](https://build123d.readthedocs.io).
Autodesk Fusion is used as a **viewer** for the generated STEP files, not as the
master copy. See [docs/decisions/0001-code-first-cad.md](docs/decisions/0001-code-first-cad.md)
for why.

## Status

| Milestone | State |
|---|---|
| M0 — CAD bootstrap (reference envelope) | Done |
| M1 — parameter system | Done |
| M2 — electronics placeholders | Done |
| M3 — front-layout mock-up | Ready to print, awaiting physical review |
| M4–M8 | Not started |

## Coordinate system

- **X**: left (−) / right (+), origin at the enclosure's horizontal centre
- **Y**: front (0) / rear (+), origin on the front-face plane
- **Z**: bottom (0) / top (+), origin on the bottom plane
- Millimetres throughout.

## Setup (once)

```sh
brew install uv
cd mechanical/cad
uv sync
```

`uv` installs an isolated Python 3.12 for this folder only; your system Python
is untouched.

## Build

```sh
cd mechanical/cad
uv run pytest            # geometry and parameter checks
uv run python -m waveform_one.build
```

Outputs (regenerated and overwritten on every run, not committed):

- `exports/step/waveform_one_m3_layout.step` — whole assembly; open in Fusion
- `exports/previews/waveform_one_m3_views.png` — four shaded views
- `exports/3mf/<part>_revA_PLA.3mf` and `exports/stl/…` — printable parts,
  already in print orientation

The build stops with a clear message if parameters are inconsistent, a
component leaves the interior, or anything collides with another component or
its keep-out space.

## What the model contains

- **Envelope** — 330 × 145 × 245 mm outer volume
- **Electronics** — Display, Diffuser, Matrix (P3 64×32), RaspberryPi (4B, with
  fan-heatsink allowance and mounting holes), ESP32 (S3-DevKitC-1), Microphone
  (INMP441 on the rear panel)
- **KeepOut_\*** bodies (orange) — space reserved for plugs, ribbons, cable
  bends and header wiring. Nothing else may enter them.
- **FrontLayoutMockup** — the M3 printable parts (below)
- **AlbumReference** — a 12-inch sleeve leaning back 8° from the top rear

## M3: printing the front-layout mock-up

A flat, full-size front panel to judge proportions next to your turntable. It
is not the final enclosure front: no mounting features, no calibrated fits.

### Parts

| File | Qty | Time* | Notes |
|---|---|---|---|
| `front_center_revA_PLA.3mf` | 1 | 3 h 02 m | 218 × 245 mm; fills the bed on its own |
| `front_left_revA_PLA.3mf`, `front_right_revA_PLA.3mf` | 1 each | ~1 h 29 m each | Side strips, engraved "WAVEFORM ONE" / "MARK I" |
| `front_splice_left_revA_PLA.3mf`, `front_splice_right_revA_PLA.3mf` | 1 each | 29 m each | Glue across the seams |
| `stand_left_revA_PLA.3mf`, `stand_right_revA_PLA.3mf` | 1 each | 1 h 23 m each | Hold the panel upright |

\*ElegooSlicer "0.20mm Standard @Elegoo CC 0.4 nozzle" with Elegoo PLA, each
part sliced alone. About 165 cm³ (~205 g) of PLA in total. Printing the small
parts together on one plate is quicker than the sum.

### Settings

- **Black PLA**, profile *0.20mm Standard @Elegoo CC 0.4 nozzle*.
- **No supports, no brim.** Parts open already lying in print orientation;
  don't rotate them.
- Panel pieces print **front face down**, so the visible face takes the bed's
  surface finish. The textured side of the plate gives a matte front.
- The name is engraved 0.6 mm into the front of the side strips (Futura Bold,
  ~3.8 mm tall), top-aligned with the display window. Engraving, not raised
  lettering, because raised text on the bed side would need supports.
- **Print 1:** `front_center` alone (it fills the bed).
- **Prints 2 and 3:** import the other six files into one new project and press
  **A** (Arrange). ElegooSlicer splits them over two plates: one strip, both
  splices and one stand (~3 h 34 m), then the other strip and stand (~2 h 42 m).
  Print one plate, clear the bed, print the other.
- They can't share one plate: the Centauri Carbon profile keeps the front
  ~23 mm of the bed clear for its purge line, and the 245 mm strips plus the
  slicer's spacing don't fit together in what remains.

### Assembly

1. Lay the three panel pieces face-down on a flat table, seams butted tight.
2. Glue a splice plate along the back of each seam, centred on it (superglue,
   or strong double-sided tape if you want to take it apart again).
3. Stand the panel up and push its bottom edge into both stands; each stand
   straddles a seam. The slot fit is uncalibrated: wrap tape round the panel
   edge if it's loose.

### What to judge (M3 review)

Place it where Waveform One will live, with a real LP sleeve behind it.
Optionally hold the display and LED panel behind their windows.

- Overall width and height beside the turntable
- Display and LED panel size relative to the front
- The ~68 mm blank bands left and right of the windows: keep, or narrow the case?
- Gap between display and LED panel
- LP sleeve height above the unit
- Overall balance

Record the verdict in `prototype-notes/`; the envelope is frozen only after
this review.

### Viewing in Fusion

1. **Preferences → General → Design → Default modeling orientation: Z up**
   (Fusion defaults to Y up, which would show the model lying on its back).
2. **File → Open → Open from my computer…** and pick the STEP file.
3. In the browser tree, click the eye icon next to **Envelope** to see inside.

## Layout

```text
mechanical/
├── cad/                   build123d source (pyproject.toml, tests/)
│   └── waveform_one/
│       ├── parameters.py  every dimension, with unit and trust status
│       ├── build.py       entry point: validate → build → export
│       ├── layout.py      where each component sits, from parameters
│       ├── geometry.py    shared shape helpers and the Placeholder type
│       ├── validation.py  parameter, fit, collision and print-envelope checks
│       ├── export.py      STEP and preview-image writers
│       ├── components/    electronics placeholders (M2)
│       ├── enclosure/     printable parts (M3, M5)
│       └── calibration/   test coupons (M4)
├── docs/                  decisions and colour plan
├── reference/measurements/  real-hardware measurements and checklist
└── exports/               generated files only
```

## Parameter trust levels

Each parameter in `parameters.py` is tagged `VERIFIED`, `DATASHEET`,
`PROVISIONAL`, `MEASURED`, `CALIBRATED` or `POLICY` (a deliberate design rule).
Nothing is treated as measured until it has been measured on real hardware and
recorded in `reference/measurements/`.
