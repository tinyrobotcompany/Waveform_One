# LED adapter board

A small board (72 × 44 mm) that connects the P4 touchscreen board to the LED
panel, so the P4 can drive the panel without the ESP32-S3 and without loose
jumper wires. Every cable plugs in at both ends.

```text
P4 socket P3 ──[PH2.0 12-pin cable]── J1 ┐
                                         ├─ buffers ── J3 ──[panel's grey ribbon]── LED panel
P4 socket P1 ──[PH2.0 12-pin cable]── J2 ┘
                       5 V from the LED supply ── J4
```

| Connector | Takes |
|---|---|
| J1 | Cable to the P4's **P3** socket (GPIO2–5, 28–31, 34; 3.3 V and GND) |
| J2 | Cable to the P4's **P1** socket (GPIO49–52; GND) |
| J3 | The LED panel's own 16-pin ribbon (HUB75) |
| J4 | 5 V and GND from the LED panel's supply (screw terminal) |

The labels are printed on the board.

## In the case

The board lies flat on four floor posts (M3 heat-set inserts, 5 mm high) in
the left half, behind the screen and under the P4's two 12-pin sockets, with
J1 and J2 towards the back. `board.json` gives its position (`case_left_x`,
`case_rear_y`, `standoff_height`); the casing CAD reads it from
`placement.json`, as it does for the carrier board.

## What is on it

- **Two 74AHCT245 buffers** turn the P4's 3.3 V signals into the 5 V the
  panel expects. These are the same buffers as on the carrier board.
- **The panel stays dark unless the P4 is on.** Q1 enables the buffers only
  while the P4's 3.3 V is present. Otherwise R4 holds the panel's OE high.
  R1 does the same while the P4 starts.
- **5 V comes from the LED panel's supply** through J4. The buffers draw only
  a few mA, and the P4's 12-pin sockets carry no 5 V.

The signal map is Codex's, from
`docs/commissioning/2026-10-14-p4-led-takeover.md`:

| HUB75 | R1 | G1 | B1 | R2 | G2 | B2 | A | B | C | D | CLK | LAT | OE |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P4 GPIO | 2 | 3 | 4 | 5 | 28 | 29 | 30 | 31 | 34 | 49 | 52 | 50 | 51 |

The P4's socket pins come from Waveshare's 7B schematic (P1 and P3).
`tools/netlist.py` lists them pin by pin.

## Cables to buy

- **2 × PH2.0 12-pin cables with a plug at both ends, "same direction"**
  (sometimes listed as 同向 or "forward"). 10–15 cm long.
  - "Same direction" means wire 1 goes to pin 1 at the other end. The
    "reverse" kind sends wire 1 to pin 12 and **will not work**.
  - Before plugging one in, check with a multimeter that the end wires meet the
    same-numbered pins.
- The panel's grey 16-pin ribbon, which came with the panel.
- Two short wires (red and black) from the LED supply's screw terminal to J4.

## Order from JLCPCB

```sh
python3 electronics/led-adapter/tools/export_jlc.py
```

This writes `fab/`. On jlcpcb.com, follow the carrier board's steps
(`electronics/carrier/README.md`, "Order from JLCPCB"):

1. Upload `led-adapter-gerbers.zip`. Use the defaults: 2 layers, 1.6 mm,
   quantity 5.
2. Turn on PCB Assembly, top side. J1–J4 are through-hole, so choose
   **Standard** assembly with through-hole soldering.
3. Upload `led-adapter-bom.csv` and `led-adapter-cpl.csv`. Check that every
   part is matched and in stock.
4. In the placement preview, check that each part sits the right way round.
   Compare it with KiCad's render (`kicad-cli pcb render --side top`):
   - pin 1 of U1, U2, Q1, J1, J2 and J3;
   - the wire entries of J4, which face the board's front edge (away from the
     "+5V" and "GND" text), so the wires come in from the front as the case
     expects.

   Rotate any part that is wrong in JLCPCB's preview. Revision 1 was ordered
   with J4 turned the other way, and JLCPCB was asked to turn it 180° (2026-10-20);
   these files now have it the right way round.

## Regenerate

The tools reuse the carrier board's generators (`reuse.py` points them at this
folder). Don't edit the generated files by hand. Edit `tools/netlist.py` or
`board.json` instead, then run:

```sh
python3 electronics/led-adapter/tools/gen_schematic.py
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
$KP electronics/led-adapter/tools/build_board.py
$KP electronics/led-adapter/tools/route_board.py      # needs Java and Freerouting, as for the carrier
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
$K sch erc electronics/led-adapter/led-adapter.kicad_sch
$K pcb drc --schematic-parity electronics/led-adapter/led-adapter.kicad_pcb
```

## Status

- Designed, routed and checked: no design-rule violations, nothing
  unconnected, and the board matches the schematic.
- **Not yet ordered.**
- Order it only after the jumper bench test shows the panel works from the P4
  with this signal map.
