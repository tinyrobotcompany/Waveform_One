# Waveform One — mechanical

Code-first parametric CAD for the Waveform One enclosure, printed on an Elegoo
Centauri Carbon (with the CANVAS four-colour add-on).

The model is Python source using [build123d](https://build123d.readthedocs.io).
Autodesk Fusion is used as a **viewer** for the generated STEP files, not as the
master copy. See [docs/decisions/0001-code-first-cad.md](docs/decisions/0001-code-first-cad.md)
for why.

## Status

| Milestone | State |
|---|---|
| M0 — CAD bootstrap (reference envelope) | Implemented, awaiting review |
| M1–M8 | Not started |

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

- `exports/step/waveform_one_m0_reference.step` — open this in Fusion
- `exports/previews/waveform_one_m0_iso.svg` — line-drawing preview

### Viewing in Fusion

1. **Preferences → General → Design → Default modeling orientation: Z up**
   (Fusion defaults to Y up, which would show the model lying on its back).
2. **File → Open → Open from my computer…** and pick the STEP file.

## Layout

```text
mechanical/
├── cad/                   build123d source (pyproject.toml, tests/)
│   └── waveform_one/
│       ├── parameters.py  every dimension, with unit and trust status
│       ├── build.py       entry point: validate → build → export
│       ├── validation.py  parameter and print-envelope checks
│       ├── export.py      STEP / preview writers
│       ├── components/    electronics placeholders (M2)
│       ├── enclosure/     printable parts (M3, M5)
│       └── calibration/   test coupons (M4)
├── docs/                  mechanical decisions and colour plan
├── reference/measurements/  real-hardware measurements and checklist
└── exports/               generated files only
```

## Parameter trust levels

Each parameter in `parameters.py` is tagged `VERIFIED`, `DATASHEET`,
`PROVISIONAL`, `MEASURED`, `CALIBRATED` or `POLICY` (a deliberate design rule).
Nothing is treated as measured until it has been measured on real hardware and
recorded in `reference/measurements/`.
