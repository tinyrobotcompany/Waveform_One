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
| M1 — parameter system | Implemented, awaiting review |
| M2 — electronics placeholders | Implemented, awaiting review |
| M3–M8 | Not started |

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

- `exports/step/waveform_one_m2_layout.step` — open this in Fusion
- `exports/previews/waveform_one_m2_views.png` — four shaded views

The build stops with a clear message if parameters are inconsistent, a
component leaves the interior, or anything collides with another component or
its keep-out space.

## What M2 contains

Reference only; nothing here is printable yet.

- **Envelope** — 330 × 145 × 245 mm outer volume
- **Electronics** — Display, Diffuser, Matrix (P3 64×32), RaspberryPi (4B, with
  fan-heatsink allowance and mounting holes), ESP32 (S3-DevKitC-1), Microphone
  (INMP441 on the rear panel)
- **KeepOut_\*** bodies (orange) — space reserved for plugs, ribbons, cable
  bends and header wiring. Nothing else may enter them.

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
