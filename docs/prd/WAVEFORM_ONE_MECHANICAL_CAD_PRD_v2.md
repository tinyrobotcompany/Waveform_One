# WAVEFORM ONE — Mechanical CAD & 3D Printing PRD

## 1. Document purpose

This document defines the mechanical design, CAD automation, prototyping, 3D-printing, assembly, validation and iteration requirements for the **Waveform One** enclosure and related mechanical parts.

It is the mechanical source of truth for all work under:

```text
mechanical/
```

The intent is to develop the Waveform One enclosure as a **fully parametric, code-first Autodesk Fusion design**, generated through the Fusion Python API, version-controlled in Git, validated incrementally, and printed on an **Elegoo Centauri Carbon**.

This document is deliberately scoped to the mechanical/3D-printing phase. It does not redefine the electronics, firmware, Raspberry Pi software, recognition architecture or social-media strategy already defined in the main Waveform One architecture PRD.

---

# 2. Core design principles

The mechanical development process must follow these principles:

1. **Fusion 360 is the authoritative CAD environment.**
2. **Geometry is generated programmatically using the Autodesk Fusion Python API wherever practical.**
3. **All important dimensions must be centralized as parameters.**
4. **Do not hard-code engineering dimensions inside geometry-generation functions.**
5. **The CAD model must be reproducible from source code.**
6. **Every physical subsystem must be modeled as an independent Fusion component.**
7. **The final enclosure must be modular because the printer build volume is smaller than the product width.**
8. **Do not attempt to generate the complete finished enclosure in the first implementation.**
9. **The build must progress through explicitly defined mechanical milestones.**
10. **Every milestone must have clear acceptance criteria before the next milestone begins.**
11. **Print small test coupons before printing large enclosure parts.**
12. **Real component measurements override datasheet assumptions once hardware arrives.**
13. **Mechanical design decisions must prioritize assembly, serviceability, cooling and manufacturability before aesthetics.**
14. **Industrial-design refinements come only after the engineering enclosure is proven.**

---

# 3. Product mechanical intent

Waveform One is a tabletop hi-fi-style appliance designed to sit near a turntable or audio system.

The product should visually feel like a piece of premium audio equipment rather than a hobby electronics enclosure.

Primary visual characteristics:

- compact but substantial proportions
- strong horizontal composition
- matte dark front surfaces
- restrained industrial design
- real or simulated walnut side details in later versions
- aluminium-style rotary controls
- separate visible regions for:
  - metadata display
  - RGB visualizer
  - rotary controls
- integrated support for a 12-inch LP sleeve behind the unit
- minimal visible fasteners from the front
- removable rear/service panels

The initial engineering prototype is not required to be visually polished.

---

# 4. Printer constraints

## Printer

**Manufacturer:** Elegoo  
**Model:** Centauri Carbon

## Build volume

```text
256 × 256 × 256 mm
```

## Nozzle

```text
0.4 mm hardened steel
```

## Slicer

```text
ElegooSlicer
```

## Prototype material

```text
PLA
```

## Engineering enclosure material

```text
PETG
```

## Possible final-material candidates

To be evaluated later:

```text
PETG
ASA
PETG-CF
```

PLA is preferred for the earliest stages because it is fast, dimensionally predictable and easy to iterate.

PETG is preferred for functional enclosure prototypes because it is tougher, less brittle and more heat resistant.

ASA or PETG-CF may be evaluated only after the geometry and assembly strategy are stable.

---

# 5. Print envelope policy

No designed printable part may intentionally consume the entire nominal printer build volume.

Use these engineering limits:

```python
PRINTER_BUILD_X = 256.0
PRINTER_BUILD_Y = 256.0
PRINTER_BUILD_Z = 256.0

MAX_PRINT_X = 245.0
MAX_PRINT_Y = 245.0
MAX_PRINT_Z = 245.0
```

The 245 mm engineering envelope provides margin for:

- brims
- orientation changes
- slicer limits
- bed-edge effects
- calibration variation
- support geometry
- printer-specific margins

Any generated part exceeding the engineering envelope must be flagged before export.

---

# 6. Nominal Waveform One envelope

The current product target is:

```text
Width:  330 mm
Height: 245 mm
Depth:  145 mm
```

These dimensions are provisional until the first physical front-layout prototype is reviewed.

A standard 12-inch LP sleeve is approximately:

```text
315 × 315 mm
```

The enclosure width is deliberately close to LP-sleeve width.

Because the enclosure width exceeds the 256 mm printer build volume, the enclosure must be designed as a **multi-part modular assembly**.

---

# 7. Coordinate system and datum conventions

The Fusion model must use a consistent coordinate system.

Recommended:

```text
X = left ↔ right
Y = front ↔ rear
Z = bottom ↔ top
```

Origin:

```text
X = enclosure horizontal center
Y = front face reference plane
Z = bottom reference plane
```

Recommended front-face convention:

```text
Y = 0
```

Positive Y extends toward the rear of the enclosure.

This convention must remain stable throughout the entire project.

---

# 8. Repository structure

Waveform One uses a **single monorepo**. Mechanical CAD, electronics, firmware, Raspberry Pi software, protocols, manufacturing assets and documentation must remain clearly separated while sharing one version history.

The repository root is:

```text
/Users/simonholmes/Projects/Applications/Waveform_One
```

The required repository structure is:

```text
Waveform_One/
│
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── CHANGELOG.md
│
├── docs/
│   ├── architecture/
│   ├── adr/
│   ├── prd/
│   │   ├── WAVEFORM_ONE_SYSTEM_PRD.md
│   │   ├── WAVEFORM_ONE_MECHANICAL_CAD_PRD.md
│   │   └── waveform_one_precise_purchasing_bom_v1.md
│   ├── assembly/
│   ├── testing/
│   └── social/
│
├── mechanical/
│   ├── README.md
│   ├── fusion/
│   │   └── WaveformOne/
│   │       ├── WaveformOne.py
│   │       ├── parameters.py
│   │       ├── geometry.py
│   │       ├── validation.py
│   │       ├── components/
│   │       │   ├── hub75.py
│   │       │   ├── lcd.py
│   │       │   ├── raspberry_pi.py
│   │       │   ├── esp32.py
│   │       │   ├── encoder.py
│   │       │   └── microphone.py
│   │       ├── enclosure/
│   │       │   ├── front_left.py
│   │       │   ├── front_center.py
│   │       │   ├── front_right.py
│   │       │   ├── chassis_left.py
│   │       │   ├── chassis_right.py
│   │       │   ├── rear_panel.py
│   │       │   ├── album_support.py
│   │       │   ├── electronics_tray.py
│   │       │   └── joints.py
│   │       ├── calibration/
│   │       │   ├── insert_coupon.py
│   │       │   ├── clearance_coupon.py
│   │       │   ├── screw_coupon.py
│   │       │   └── joint_coupon.py
│   │       └── export.py
│   ├── reference/
│   │   ├── datasheets/
│   │   ├── step/
│   │   ├── measurements/
│   │   └── photos/
│   ├── exports/
│   │   ├── step/
│   │   ├── stl/
│   │   └── 3mf/
│   ├── slicer/
│   │   └── ElegooSlicer/
│   └── prototype-notes/
│
├── electronics/
│   ├── README.md
│   ├── kicad/
│   │   ├── control-board/
│   │   └── power-board/
│   ├── schematics/
│   ├── pcb/
│   ├── gerbers/
│   ├── bom/
│   ├── datasheets/
│   └── bringup/
│
├── firmware/
│   └── esp32/
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       ├── main/
│       │   ├── main.cpp
│       │   ├── audio/
│       │   ├── display/
│       │   ├── controls/
│       │   ├── protocol/
│       │   └── system/
│       └── test/
│
├── pi/
│   ├── core/
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── recognition/
│   │       ├── metadata/
│   │       ├── artwork/
│   │       ├── cache/
│   │       ├── device/
│   │       ├── state/
│   │       └── config/
│   ├── ui/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── qml/
│   │   └── assets/
│   └── systemd/
│
├── protocol/
│   ├── specification.md
│   ├── messages.md
│   └── test-vectors/
│
├── manufacturing/
│   ├── assembly/
│   ├── test-fixtures/
│   ├── flashing/
│   ├── calibration/
│   └── packaging/
│
├── scripts/
│   ├── build/
│   ├── release/
│   ├── diagnostics/
│   └── manufacturing/
│
├── tests/
│   ├── integration/
│   ├── hardware-in-loop/
│   └── system/
│
└── .github/
    └── workflows/
```

## Repository rules

1. **Requirements live under `docs/prd/`.**
2. **Mechanical source code lives under `mechanical/fusion/`.**
3. **Generated mechanical outputs must never be mixed with Fusion source code.**
   - STEP → `mechanical/exports/step/`
   - STL → `mechanical/exports/stl/`
   - 3MF → `mechanical/exports/3mf/`
4. **KiCad sources live under `electronics/kicad/`; manufacturing outputs live separately under `electronics/gerbers/`.**
5. **Claude/Codex working on Mechanical M0–M8 should remain inside `mechanical/` and `docs/prd/` unless explicitly instructed otherwise.**
6. **The current Markdown documents already present in the repository root should be moved into `docs/prd/` during repository initialization, preserving their contents.**
7. **Generated binaries, Fusion temporary files, slicer caches, build products and OS metadata must be excluded through `.gitignore`.**
8. **Do not create multiple independent repositories for mechanical, electronics and software. Their revisions must remain synchronized in this monorepo.**


---

# 9. Fusion code architecture

The Fusion add-in/script must be modular.

## Entry point

```text
WaveformOne.py
```

Responsibilities:

- initialize Fusion API
- load parameters
- create or reset generated components
- call generation functions
- run validation
- optionally export selected bodies/components
- report errors clearly

## Parameters

```text
parameters.py
```

Must contain all major engineering values.

No geometry-generation function should contain unexplained hard-coded dimensions.

## Geometry helpers

```text
geometry.py
```

Should include reusable helpers for:

- sketches
- rectangles
- circles
- extrusions
- cut-outs
- fillets
- chamfers
- hole creation
- bosses
- mounting patterns
- tongue-and-groove joints
- alignment features

## Validation

```text
validation.py
```

Must validate:

- printable envelope
- minimum wall thickness
- minimum clearance
- component overlaps
- enclosure-bound violations
- unsupported provisional dimensions
- invalid parameter combinations

---

# 10. Parameter-management requirements

Every important dimension must be marked as one of:

```text
VERIFIED
DATASHEET
PROVISIONAL
MEASURED
CALIBRATED
```

Example:

```python
CASE_WIDTH = 330.0               # PROVISIONAL
CASE_HEIGHT = 245.0              # PROVISIONAL
CASE_DEPTH = 145.0               # PROVISIONAL

MATRIX_WIDTH = 256.0             # DATASHEET
MATRIX_HEIGHT = 128.0            # DATASHEET

M3_INSERT_HOLE = None            # CALIBRATE
LCD_BODY_WIDTH = None            # MEASURE WHEN HARDWARE ARRIVES
```

Once a physical component is measured, the measurement source should be documented in:

```text
reference/measurements/
```

---

# 11. Initial master parameter set

The starting parameter file should include at least:

```python
# Product envelope
CASE_WIDTH = 330.0
CASE_HEIGHT = 245.0
CASE_DEPTH = 145.0

# Printing
MAX_PRINT_X = 245.0
MAX_PRINT_Y = 245.0
MAX_PRINT_Z = 245.0

# Shell
WALL_THICKNESS = 3.0
FRONT_BEZEL_THICKNESS = 4.0
REAR_PANEL_THICKNESS = 3.0

# HUB75
MATRIX_WIDTH = 256.0
MATRIX_HEIGHT = 128.0
MATRIX_CLEARANCE = 0.5

# LP sleeve
ALBUM_WIDTH = 315.0
ALBUM_SLOT_WIDTH = 318.0
ALBUM_SLOT_DEPTH = 10.0
ALBUM_ANGLE_DEG = 8.0

# Controls
ENCODER_COUNT = 3
ENCODER_SPACING = 38.0
ENCODER_SHAFT_DIAMETER = 6.0

# Joint assumptions
JOINT_CLEARANCE = 0.25
ALIGNMENT_PIN_CLEARANCE = 0.20

# Fasteners
M3_CLEARANCE_HOLE = 3.2
M3_INSERT_HOLE = None

# General fit
GENERAL_COMPONENT_CLEARANCE = 0.5
```

Unset values must not silently default to guessed values.

---

# 12. Electronic placeholder components

Before the real electronics arrive, model placeholder components.

Required placeholders:

```text
HUB75 P4 64×32
Waveshare 5-inch LCD
Raspberry Pi 5
ESP32-S3 DevKitC-1
three Bourns rotary encoders
microphone module
```

The placeholders must:

- exist as separate Fusion components
- use known datasheet dimensions where available
- clearly indicate provisional dimensions
- include keep-out envelopes around connectors
- include simplified mounting-hole positions where known
- avoid cosmetic details

The placeholders are for enclosure engineering, not product rendering.

---

# 13. Enclosure component architecture

Waveform One must be assembled from modular printable parts.

The current recommended component hierarchy is:

```text
WaveformOne
│
├── FrontAssembly
│   ├── FrontLeft
│   ├── FrontCenter
│   └── FrontRight
│
├── Chassis
│   ├── ChassisLeft
│   └── ChassisRight
│
├── RearPanel
│
├── AlbumSupport
│
└── ElectronicsTray
```

The exact front split is not frozen until the front-layout study is complete.

The design objective is:

- no visible ugly center seam across a major visual feature
- hidden alignment geometry
- rear-installed screws
- replaceable individual front sections if practical
- strong mechanical registration between sections

---

# 14. Modular joint strategy

Printed enclosure sections should not rely solely on glue.

Preferred joint design:

```text
tongue-and-groove
+
alignment pins
+
rear screws
+
heat-set inserts
```

Concept:

```text
PART A                      PART B

──────────────┐          ┌──────────────
              │          │
              └────┐ ┌───┘
                   │ │
                   │ │
              ┌────┘ └───┐
──────────────┘          └──────────────
```

The final front surface should minimize visible seams.

Joint geometry must be validated using calibration coupons before full-part printing.

---

# 15. Fastening strategy

Preferred fastening:

```text
M3 machine screws
+
heat-set brass inserts
```

Use heat-set inserts for:

- removable rear panel
- electronics trays
- structural panel joints
- repeated-service locations

Avoid self-tapping directly into printed plastic for parts expected to be repeatedly opened.

Threaded insert hole diameter must be calibrated on the actual printer/material combination.

---

# 16. Print-orientation requirements

The CAD must consider print orientation from the beginning.

Design goals:

- minimize supports
- avoid large unsupported horizontal roofs
- keep visible front surfaces away from support scars
- place layer lines in mechanically sensible directions
- avoid weak fastener bosses aligned with layer separation
- ensure insert bosses have sufficient surrounding material

Every exportable part should document its recommended print orientation.

---

# 17. Baseline slicing assumptions

Initial baseline:

```text
Nozzle:             0.4 mm
Layer height:       0.20 mm
Wall count:         4
Top layers:         5
Bottom layers:      5
Prototype infill:   15–20%
Structural infill:  20–30%
Supports:           avoid where practical
```

These are starting assumptions, not immutable requirements.

Temperatures and speeds remain filament-profile dependent and should be controlled in ElegooSlicer.

---

# 18. Mechanical milestone M0 — Fusion scripting bootstrap

## Goal

Prove that code can reproducibly generate geometry in Fusion.

## Required output

Create:

```text
WaveformOne
└── Envelope
```

Generate a simple reference body representing:

```text
330 × 145 × 245 mm
```

## Acceptance criteria

- script runs successfully from a clean Fusion document
- geometry appears at the correct origin
- X/Y/Z orientation matches this PRD
- dimensions are correct
- rerunning does not create duplicate uncontrolled geometry
- repository structure is committed

No additional enclosure geometry should be created in M0.

---

# 19. Mechanical milestone M1 — parameter system

## Goal

Create the centralized mechanical parameter system.

## Requirements

- implement `parameters.py`
- classify values by verification state
- eliminate hard-coded engineering dimensions
- expose parameters to geometry-generation modules
- add validation for unset required values

## Acceptance criteria

- changing `CASE_WIDTH` changes generated envelope width
- no geometry module contains unexplained major dimensional constants
- invalid parameter combinations fail clearly

---

# 20. Mechanical milestone M2 — electronic placeholders

## Goal

Represent the internal electronics without yet creating an enclosure.

## Required components

- HUB75 matrix
- LCD
- Raspberry Pi 5
- ESP32 DevKit
- three rotary encoders
- microphone

## Acceptance criteria

- each item is a separate Fusion component
- components are positioned in the intended front/internal layout
- placeholder dimensions are clearly identified as provisional or datasheet-derived
- no enclosure shell exists yet

This milestone is the first major visual design review.

---

# 21. Mechanical milestone M3 — front-layout prototype

## Goal

Validate the physical visual proportions.

Generate a flat front-layout prototype approximately:

```text
330 × 245 × 3–4 mm
```

It should contain:

- LCD opening/reference
- HUB75 opening/reference
- three encoder holes
- approximate final spacing
- LP sleeve visual reference

Because the full 330 mm width exceeds printer capacity, the front-layout print must be segmented.

Recommended:

```text
FrontLeft
FrontCenter
FrontRight
```

or a two-piece solution if geometry permits.

## Acceptance criteria

After printing:

- place next to the intended turntable/audio system
- position a 12-inch LP sleeve behind it
- visually assess proportions
- confirm:
  - width
  - height
  - LCD scale
  - matrix scale
  - encoder placement
  - album height
  - overall visual balance

The outer envelope is not considered frozen until this review passes.

---

# 22. Mechanical milestone M4 — calibration coupons

Before any detailed enclosure mounting geometry, print calibration parts.

## Heat-set insert coupon

Test candidate insert-hole diameters.

Example:

```text
4.0
4.1
4.2
4.3
4.4
4.5 mm
```

Record the best result separately for PLA and PETG if necessary.

## M3 screw clearance coupon

Test:

```text
2.9
3.0
3.1
3.2
3.3 mm
```

## Joint-clearance coupon

Test tongue-and-groove clearances:

```text
0.15
0.20
0.25
0.30
0.35 mm
```

## Alignment-pin coupon

Test:

```text
0.10
0.15
0.20
0.25
0.30 mm
```

## Acceptance criteria

Measured successful values must replace provisional values in `parameters.py`.

No full structural enclosure should be printed before these values are established.

---

# 23. Mechanical milestone M5 — engineering enclosure V1

## Goal

Create the first complete functional enclosure.

Required components:

```text
FrontLeft
FrontCenter
FrontRight
ChassisLeft
ChassisRight
RearPanel
AlbumSupport
ElectronicsTray
```

Engineering enclosure V1 must prioritize:

- component fit
- assembly sequence
- cable routing
- screw access
- structural integrity
- ventilation
- serviceability
- printability

It does not need to be aesthetically final.

---

# 24. Front assembly requirements

The front must accommodate:

- Waveshare 5-inch LCD
- 64×32 P4 HUB75 matrix
- three rotary encoders

Nominal design:

```text
┌─────────────────────────────────────┐
│  LCD                      ○         │
│                           ○         │
│                           ○         │
├─────────────────────────────────────┤
│                                     │
│          HUB75 MATRIX               │
│                                     │
└─────────────────────────────────────┘
```

Requirements:

- LCD and matrix slightly recessed behind bezel
- no visible rough print edge around display openings
- bezel lip must hide component-edge imperfections
- removable display mounting carriers preferred
- controls must be mechanically independent of decorative knobs

---

# 25. Album-support requirements

Nominal LP support:

```text
Sleeve width: 315 mm
Slot width:   318 mm
Slot depth:   10 mm
Angle:        8° rearward
```

The support must:

- hold a standard LP sleeve safely
- avoid damaging sleeve edges
- prevent excessive backward tilt
- be removable
- not obstruct rear service access
- not interfere with ventilation
- support sleeves of slightly varying thickness

A replaceable slot insert is preferred.

---

# 26. Internal electronics layout

Current internal concept:

```text
SIDE VIEW

                album sleeve
                    /
                   / 8°
                  /
        ┌────────/──────────────────┐
        │       sleeve slot         │
        │                           │
        │ LCD      control PCB      │
        │                           │
        │ MATRIX      Raspberry Pi  │
        │              + cooler     │
        │                           │
        │ power distribution PCB    │
        └───────────────────────────┘
```

The CAD must reserve clearances for:

- HDMI connector
- Pi USB connectors
- ESP32 USB cable
- HUB75 ribbon cable
- LED power wiring
- encoder wiring
- microphone cable
- cooling air path

Cable bend radii must be modeled as keep-out regions where necessary.

---

# 27. Electronics tray

The electronics tray should:

- be independently removable
- mount using M3 screws and inserts
- allow Pi installation/removal without dismantling the front fascia
- provide cable tie/strain relief points
- avoid obstructing air flow
- support future replacement of custom PCB revisions

Prefer a tray-based architecture rather than directly mounting every component to the enclosure shell.

---

# 28. Rear-panel requirements

The rear panel should provide provisional locations for:

```text
power
service USB
Ethernet access
ventilation
future line-level input
```

Requirements:

- removable independently
- secured with machine screws
- no clips as the sole structural retention mechanism
- ventilation integrated into the panel design
- no mains-voltage openings
- low-voltage power input only
- allow future connector changes without redesigning the whole enclosure

---

# 29. Ventilation requirements

Airflow path should support:

```text
side/lower intake
      ↓
Raspberry Pi
      ↓
matrix electronics
      ↓
rear/top exhaust
```

Requirements:

- no large decorative vent openings until thermal behavior is measured
- reserve ventilation zones early
- avoid directing hot exhaust toward the LP sleeve
- avoid microphone placement directly in strong airflow
- no fan grille immediately adjacent to visually dominant front surfaces

---

# 30. Microphone mechanical requirements

The microphone must not be buried deep inside the enclosure.

Preferred location:

```text
rear/top acoustic opening
```

Requirements:

- removable microphone daughterboard
- acoustic opening aligned with sensor port
- avoid direct airflow
- avoid proximity to HUB75 clock circuitry
- avoid placing directly against a vibrating large panel
- permit later replacement with another production microphone module

---

# 31. Mechanical milestone M6 — hardware arrival and measurement update

Once electronics arrive, stop relying on provisional dimensions.

Measure using digital calipers:

- LCD external dimensions
- LCD bezel
- LCD mounting holes
- LCD PCB protrusions
- matrix frame dimensions
- matrix mounting-hole positions
- matrix depth
- Raspberry Pi dimensions
- Raspberry Pi mounting holes
- active-cooler height
- ESP32 dimensions
- encoder body
- encoder threaded bushing
- encoder shaft
- cable connector protrusions
- HDMI cable bend requirements
- USB cable bend requirements

Record every measurement in:

```text
reference/measurements/
```

Update parameter status:

```text
PROVISIONAL → MEASURED
```

---

# 32. Mechanical milestone M7 — full electronics fit prototype

Regenerate the enclosure using measured dimensions.

Print the complete engineering enclosure.

Install real electronics.

Validate:

- all parts fit
- connectors remain accessible
- cables can be installed without extreme bending
- electronics can be removed
- rear panel can be removed independently
- sleeve can be inserted/removed
- knobs have appropriate spacing
- enclosure sits flat
- no component rattles
- no visible unintended gaps
- no cable is trapped by panel assembly
- airflow is unobstructed

Expect at least one geometry revision after this step.

---

# 33. Mechanical milestone M8 — industrial-design refinement

Only after M7 passes should aesthetic refinement begin.

Possible refinements:

- softer corner radii
- recessed front surfaces
- stronger visual separation between display and matrix
- hidden seam treatments
- walnut side panels
- aluminium knob geometry
- integrated branding
- decorative ventilation
- improved rear-panel composition
- refined feet
- final surface finish

No aesthetic choice may reduce serviceability or compromise thermal performance without explicit review.

---

# 34. Industrial-design material direction

Target visual language:

```text
matte black front
walnut side accents
aluminium knobs
warm white interface typography
RGB only in visualizer
```

Avoid:

- gamer-style RGB styling
- unnecessary visible fasteners
- fake wood-grain filament as the primary final finish
- overly complex decorative geometry
- glossy plastic surfaces

---

# 35. Test-print strategy

Before printing a large part:

1. identify the risky interface
2. isolate it
3. generate a small test coupon
4. print it
5. measure it
6. update parameters
7. only then print the large assembly

Examples:

- display mounting lip
- matrix recess
- encoder bushing hole
- insert boss
- panel joint
- rear screw boss
- cable pass-through

This workflow is mandatory for costly or long-duration prints.

---

# 36. Dimensional-tolerance policy

Initial assumptions before calibration:

```text
General loose-fit clearance:       0.50 mm
Sliding fit per side:              0.20–0.30 mm
Tongue/groove per side:            0.20–0.30 mm
Decorative bezel clearance:        0.30–0.50 mm
M3 clearance hole:                 3.2 mm provisional
```

Calibrated values override these defaults.

Do not assume the same values behave identically in PLA, PETG and ASA.

---

# 37. Minimum geometry rules

Initial design rules:

```text
Minimum general wall:        3.0 mm
Front bezel:                 4.0 mm nominal
Minimum boss wall:           2.0 mm around insert
Minimum unsupported lip:     minimize / validate
Minimum external fillet:     1.0 mm where appropriate
```

Structural regions may require greater thickness.

Thin decorative walls should not be used without print validation.

---

# 38. Design-for-assembly requirements

The complete enclosure must have a documented assembly sequence.

Target:

```text
1. assemble front sections
2. install LCD
3. install HUB75 matrix
4. install encoders
5. install chassis
6. install electronics tray
7. connect internal harnesses
8. install Pi
9. install ESP32/control hardware
10. install album support
11. install rear panel
12. install knobs
```

No assembly step should require removing a component already permanently installed unless unavoidable.

---

# 39. Serviceability requirements

The design should allow:

- Pi removal
- ESP32 replacement
- control PCB replacement
- display replacement
- matrix replacement
- microphone replacement
- rear connector access

without destroying printed parts.

Heat-set inserts and modular carriers should be used accordingly.

---

# 40. Export requirements

Each printable component must support export to:

```text
STEP
STL
3MF
```

STEP is the archival/mechanical exchange format.

3MF is preferred for slicer workflows where supported.

STL remains available for compatibility.

Exports must be stored under:

```text
mechanical/exports/
```

Generated filenames should include:

```text
part_name
revision
material_target
```

Example:

```text
front_left_revA_PLA.3mf
```

---

# 41. CAD regeneration requirements

The Fusion script must be safe to rerun.

Acceptable strategies:

- delete/recreate only generated components
- update named generated components deterministically
- never silently duplicate geometry

Generated components must use stable names.

Example:

```text
WFO_FRONT_LEFT
WFO_FRONT_CENTER
WFO_FRONT_RIGHT
WFO_CHASSIS_LEFT
WFO_CHASSIS_RIGHT
WFO_REAR_PANEL
WFO_ALBUM_SUPPORT
WFO_ELECTRONICS_TRAY
```

---

# 42. Validation automation

`validation.py` should eventually enforce:

- no printable part exceeds 245 mm envelope
- no unset critical parameters
- no negative or zero wall thickness
- no invalid joint clearances
- no enclosure part outside nominal bounds
- no component collisions where collision checking is practical
- no duplicate generated component names

Validation failures must stop export.

---

# 43. Codex implementation rules

The coding agent must obey the following:

1. Read this PRD before modifying mechanical code.
2. Do not generate the entire enclosure in a single task.
3. Implement only the requested milestone.
4. Do not proceed to the next milestone without explicit approval.
5. Centralize all engineering dimensions.
6. Do not invent missing hardware dimensions.
7. Mark unknown dimensions as provisional.
8. Do not silently substitute parts.
9. Do not redesign the overall product architecture.
10. Generate clear logging for every major Fusion operation.
11. Make every script safe to rerun.
12. Prefer simple deterministic geometry over clever abstractions.
13. Add comments explaining datum and coordinate conventions.
14. Keep generated component names stable.
15. Run mechanical validation before export.
16. Commit one milestone at a time.
17. Do not optimize for fewer files at the expense of maintainability.
18. Never assume a part fits the Centauri Carbon unless validation confirms it.

---

# 44. First Codex task

The first coding task should be:

> Create the mechanical project structure defined in the Waveform One Mechanical CAD PRD. Implement Mechanical Milestone M0 only. Create a Fusion Python script that, when executed in a clean Fusion document, creates a single `WaveformOne` root component with an `Envelope` reference body measuring 330 mm wide × 145 mm deep × 245 mm high using the coordinate system defined in the PRD. The design must use centralized parameters and must be safe to rerun without creating duplicate geometry. Do not create any enclosure, electronics placeholders, front panel, mounting features or other geometry yet.

Acceptance criteria:

- project files exist
- Fusion script runs
- model uses correct axes
- dimensions are correct
- rerun is deterministic
- no additional geometry exists
- code is committed before M1

---

# 45. Review loop

Every milestone should follow:

```text
PRD requirement
      ↓
Codex implementation
      ↓
Fusion generation
      ↓
visual inspection
      ↓
screenshot / review
      ↓
physical print where required
      ↓
measurement
      ↓
parameter update
      ↓
commit
      ↓
next milestone
```

The Fusion model is not considered correct merely because the script runs.

Physical validation remains mandatory.

---

# 46. Mechanical development sequence

```text
M0
Fusion scripting bootstrap
       ↓
M1
Parameter system
       ↓
M2
Electronic placeholders
       ↓
M3
Front-layout physical prototype
       ↓
M4
Calibration coupons
       ↓
M5
Engineering enclosure V1
       ↓
M6
Real hardware measurement update
       ↓
M7
Full electronics fit prototype
       ↓
M8
Industrial-design refinement
       ↓
FINAL
Waveform One V1 enclosure
```

---

# 47. Definition of done

The mechanical V1 is complete when:

- all electronics fit without force
- all cables route safely
- every serviceable subsystem can be accessed
- the enclosure fits the Centauri Carbon print constraints
- all large parts have repeatable print orientation
- all structural joints are mechanically secured
- no critical part relies solely on adhesive
- rear service access works
- thermal testing shows acceptable temperatures
- LP sleeve support works reliably
- front layout meets visual-design expectations
- enclosure dimensions are frozen
- final STEP/3MF/STL exports exist
- assembly documentation exists
- Fusion source scripts reproduce the complete design
- all critical dimensions are measured or calibrated rather than guessed
- no unresolved mechanical acceptance issues remain

---

# 48. Current status

```text
Mechanical project status:
READY TO START M0
```

Current known printer configuration:

```text
Printer:          Elegoo Centauri Carbon
Build volume:     256 × 256 × 256 mm
Nozzle:           0.4 mm hardened steel
Slicer:           ElegooSlicer
Prototype:        PLA
Functional test:  PETG
CAD:              Autodesk Fusion
CAD automation:   Fusion Python API
Development:      Codex CLI
Source control:   Git
```

The immediate next action is to give this PRD to Codex CLI and implement **Mechanical Milestone M0 only**.
