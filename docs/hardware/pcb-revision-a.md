# Revision A carrier PCB — design inputs, not a fabrication release

Status: proposed architecture. Do not order from this document; no checked
schematic, routed PCB, Gerbers, drill files or assembly package exists yet.

## Proposed architecture

Retain the Raspberry Pi 4 and ESP32-S3 module/controller. A carrier PCB replaces
loose signal wiring, provides keyed connectors and mounting, and manages low-voltage
power distribution. The Pi runs UI/recognition/update services; the ESP drives HUB75
and samples the microphone. Preserve the internal USB connection for updates.

Use the verified GPIO mapping in `firmware/esp32/README.md`, not old prototype BOM
assumptions. The historical purchasing BOM specifies Pi 5, a 5-inch display and an
8 MB ESP; the running prototype instead has Pi 4, a reported 7-inch display and
16 MB ESP. Those differences must be resolved before footprints or enclosure
clearances are frozen.

## Power and thermal design requirements

- Use an external certified low-voltage supply; no mains circuitry on this board.
- Measure/obtain worst-case current for the exact LED panel, Pi, LCD, ESP and fan.
  Select supply, conductors, connectors, fuse/eFuse and copper from that budget
  with transient margin, not from average music brightness.
- Separate protected branches for the high-current LED load and Pi/controller,
  with deliberate common-ground returns and local bulk/decoupling capacitors.
- Prevent USB/backfeed paths when the ESP or Pi can have an independent supply.
- Provide controlled Pi shutdown before power removal; ordinary power switches
  are not sufficient for a writable Linux filesystem.
- Reserve a keyed HUB75 interface, test points for supply/ground/clock and reset,
  and debug/recovery access. Validate the panel's required input thresholds and
  the final 3.3 V to panel-level buffering circuit before layout.
- Keep switching/high-current paths away from the microphone and preserve its
  acoustic opening. Validate USB routing and module antenna keep-out requirements.
- Measure temperatures under worst-case load in the final enclosure. Identify the
  existing heatsink/fan HAT before reserving height, GPIO and fan power. One open-air
  51.1 C reading does not prove enclosed thermal performance.

## Inputs still required

Exact LCD and LED panel models/revisions; ESP board/module part number and dimensions;
all supply label ratings and present wiring topology; existing cooling HAT model;
connector preferences; approved enclosure mounting/height limits. Coordinate with
mechanical work rather than changing its separate worktree.

## Manufacturing deliverables

KiCad schematic and PCB sources, ERC/DRC results, board stack-up and fabrication
notes, Gerber and Excellon drill package. If PCBWay assembles the board, also provide
BOM with manufacturer part numbers, placement/CPL data and assembly drawings.
Prototype validation must precede quantity ordering. PCBWay's quote form is the
manufacturing entry point, not validation of the electrical design:
https://www.pcbway.com/orderonline.aspx

## Reconciled inputs from mechanical work (read-only review)

The separate `mechanical-m0` worktree records the original Raspberry Pi 7-inch DSI
Touchscreen and Waveshare RGB-Matrix-P3-64x32. Its owner measurement on 2026-09-26
was approximately 195 × 100 × 12 mm, consistent with a nominal 192 × 96 mm P3 panel.
These are more recent inputs than the purchasing BOM; mounting measurements remain
unconfirmed. Live `/proc/device-tree/model` identifies Raspberry Pi 4 Model B Rev 1.5.
Do not use the old P4 panel or Pi 5 footprints/envelope assumptions.

Remaining electrical inputs to obtain from the owner: exact ESP dev-board marking,
LED supply voltage/current label and connection arrangement, Pi/display supply
arrangement, and cooler/HAT model. These determine connector ratings, power-tree
choices, footprint spacing, GPIO conflicts and thermal clearance.


## Owner-confirmed hardware — 27 September 2026

- Raspberry Pi 4 (live identification: Model B Rev 1.5).
- Original 7-inch Raspberry Pi touchscreen, described by owner as the Pi 3-era
  model; powered from the Pi. Confirm connector/power-wire routing during layout.
- ESP32-S3 DevKitC-1 N16R8: 16 MB flash, 8 MB PSRAM. Exact manufacturer drawing
  and physical board dimensions still needed before freezing socket footprints.
- LED panel uses an external 5 V supply. Output current rating and connector/
  cable specification remain unknown; voltage alone cannot size power protection.
- No cooling HAT currently fitted. Do not allocate a specific HAT footprint or
  assume one is installed; cooling will be selected from enclosed thermal tests.

The Pi power source rating is not yet recorded. The touchscreen's load must be
included in the Pi power budget. Do not combine the present supplies on a PCB
until the full power tree and USB backfeed prevention are checked.
