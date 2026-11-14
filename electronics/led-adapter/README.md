# LED adapter board

A small board (94 × 44 mm) that connects the P4 touchscreen board to the LED
panel, so the P4 can drive the panel without the ESP32-S3 and without loose
jumper wires. It is also the system's one power point: the 5 V supply plugs in
here, and the board powers the LED panel and the P4. Every cable plugs in at
both ends.

```text
5 V supply (barrel plug) ── J5 ┬── J4 ──[panel's own power cable]── LED panel power
                               ├── J6 ──[USB-C to USB-C cable]────── P4 USB-C port
                               └── buffers, power LED
P4 socket P3 ──[PH2.0 12-pin cable, as delivered]── J1 ┐
                                                       ├─ buffers ── J3 ──[panel's grey ribbon]── LED panel input
P4 socket P1 ──[PH2.0 12-pin cable, as delivered]── J2 ┘
```

| Connector | Takes |
|---|---|
| J1 | Cable to the P4's **P3** socket (GPIO2–5, 28–31, 34; 3.3 V and GND) |
| J2 | Cable to the P4's **P1** socket (GPIO49–52; GND) |
| J3 | The LED panel's own 16-pin ribbon (HUB75) |
| J4 | The LED panel's own power cable (screw terminal, up to 4 A) |
| J5 | The 5 V supply's barrel plug (5.5 × 2.1 mm, centre +) |
| J6 | USB-C to USB-C cable to the P4's USB-C port (power only) |

The labels are printed on the board. D2, marked POWER, lights while 5 V is
present.

## In the case

The board lies flat on four floor posts (M3 heat-set inserts, 5 mm high) in
the left half, behind the screen and under the P4's two 12-pin sockets, with
J1 and J2 towards the back. `board.json` gives its position (`case_left_x`,
`case_rear_y`, `standoff_height`); the casing CAD reads it from
`placement.json`, as it does for the carrier board.

## What is on it

- **One power input, J5**, for a 5 V supply of at least 6 A: up to 4 A for
  the panel and the rest for the P4. MEAN WELL GST60A05-P1J (5 V, 6 A,
  5.5 × 2.1 mm plug, centre +) fits. A TVS diode (D1) clamps plug-in spikes.
- **The panel's power, J4**, straight from J5 on wide copper.
- **The P4's power, J6**, through a 2.5 A resettable fuse (F1). Waveshare
  documents USB-C (or a battery) as the P4's only power inputs, so the board
  feeds it like a USB charger: 56 kΩ on CC1 and CC2 mark it as a USB power
  source. The P4's two USB-C ports each have their own input diode, so the
  Mac can stay plugged into the P4's other USB-C port for flashing.
- **Two 74AHCT245 buffers** turn the P4's 3.3 V signals into the 5 V the
  panel expects. These are the same buffers as on the carrier board.
- **The panel stays dark unless the P4 is on.** Q1 enables the buffers only
  while the P4's 3.3 V is present. Otherwise R4 holds the panel's OE high.
  R1 does the same while the P4 starts.

The signal map is Codex's, from
`docs/commissioning/2026-10-14-p4-led-takeover.md`:

| HUB75 | R1 | G1 | B1 | R2 | G2 | B2 | A | B | C | D | CLK | LAT | OE |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P4 GPIO | 2 | 3 | 4 | 5 | 28 | 29 | 30 | 31 | 34 | 49 | 52 | 50 | 51 |

The P4's socket pins come from Waveshare's 7B schematic (P1 and P3).
`tools/netlist.py` lists them pin by pin.

## Cables

### The two 12-pin cables: use them as delivered

J1 and J2 take PH2.0 12-pin double-ended cables **exactly as they come from
the bag**: the AliExpress "Double head, 12P, 20CM" cables. Don't move any wire.

Those cables are crossed: pin 1 at one end is wired to pin 12 at the other.
Revision 2 makes the cross-over on the board instead. J1 and J2 pin *n*
carries the P4 socket's pin 13 − *n*:

| P4 P3 pin | P3 label | J1 pin | | P4 P1 pin | P1 label | J2 pin |
|---|---|---|---|---|---|---|
| 1 | IO36 | 12 (unused) | | 1 | IO52 (CLK) | 12 |
| 2 | IO34 (C) | 11 | | 2 | IO51 (OE) | 11 |
| 3 | IO31 (B) | 10 | | 3 | IO50 (LAT) | 10 |
| 4 | IO30 (A) | 9 | | 4 | IO49 (D) | 9 |
| 5 | IO29 (B2) | 8 | | 5–7 | IO48–46 | 8–6 (unused) |
| 6 | IO28 (G2) | 7 | | 8 | GND | 5 |
| 7 | IO5 (R2) | 6 | | 9 | VO4 | 4 (unused) |
| 8 | IO4 (B1) | 5 | | 10 | 3V3 | 3 (unused) |
| 9 | IO3 (G1) | 4 | | 11 | GND | 2 |
| 10 | IO2 (R1) | 3 | | 12 | BAT | 1 (unused) |
| 11 | GND | 2 | | | | |
| 12 | 3V3 | 1 | | | | |

How we know: the owner's photos of the cables plugged in unmodified
(2026-11-04). At the P4's P3 socket, from IO36 to 3V3, the wires run red,
black, brown, grey, purple, blue, green, orange, yellow, white, red, black.
At revision 1's J1, from pin 1 (the end nearest J4) to pin 12, they ran in
exactly the opposite order. The P1 cable is the same.

**Check before plugging in.** Plug the cable into the P4's P3 socket and
look at the wire next to the **3V3** label: it is black. At the green board,
that same black wire must go into **J1's pin 1**, the end of J1 nearest the
board's left edge (the USB-C and power sockets). For the P1 cable: the wire at
the P4's **BAT** label goes into **J2's pin 1**, the end of J2 nearest J1.

The two cables reworked by hand for revision 1 (wires moved to run pin 1 to
pin 1) do **not** work on revision 2. Keep them with the revision 1 board.

### Power

- **5 V supply**: 5 V, at least 6 A, barrel plug 5.5 × 2.1 mm, centre +. For
  example MEAN WELL GST60A05-P1J (it needs an IEC C13 mains lead, sold
  separately).
- **USB-C to USB-C cable**, short (about 20 cm), from J6 to the P4's USB-C
  port. Any charging cable works: only power goes through it.
- **The panel's own power cable** into J4: red to **+5V**, black to **GND**,
  as printed.

### Panel signals

- The panel's grey 16-pin ribbon, which came with the panel, into J3 and the
  panel's **input** socket.

## Order from JLCPCB

```sh
python3 electronics/led-adapter/tools/export_jlc.py
```

This writes `fab/`. On jlcpcb.com, follow the carrier board's steps
(`electronics/carrier/README.md`, "Order from JLCPCB"):

1. Upload `led-adapter-gerbers.zip`. Use the defaults: 2 layers, 1.6 mm,
   quantity 5.
2. Turn on PCB Assembly, top side. J1–J5 are through-hole, so choose
   **Standard** assembly with through-hole soldering.
3. Upload `led-adapter-bom.csv` and `led-adapter-cpl.csv`. Check that every
   part is matched and in stock.
4. In the placement preview, check that each part sits the right way round.
   Compare it with KiCad's render (`kicad-cli pcb render --side top`):
   - pin 1 of U1, U2, Q1, D1, D2, J1, J2 and J3;
   - the wire entries of J4, which face the board's front edge (away from the
     "+5V" and "GND" text), so the wires come in from the front as the case
     expects;
   - the plug opening of J5, at the board's rear edge (next to J1), and the
     mouth of J6, at the board's left edge.

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

- **Revision 2** (this design): one power point, power LED, cables used as
  delivered. Designed, routed and checked: no design-rule violations,
  nothing unconnected, and the board matches the schematic. Every panel
  signal was traced from the P4 pin, through an as-delivered cable, to the
  HUB75 pin. **Ordered 2026-11-14** from JLCPCB, 5 assembled boards
  (order Y4-13817483A). J6 was fitted as LCSC C3020560, the same GCT
  USB4105-GF-A, because C3025063 was out of stock.
- **Revision 1** (ordered 2026-10-20, in use): needs the 12-pin cables
  reworked to run pin 1 to pin 1, takes 5 V for the buffers only on J4, and
  the P4 is powered separately by USB-C.
- Revision 2 is 22 mm wider on the left than revision 1; the parts from
  revision 1 sit at the same places in the case. The casing CAD follows
  `placement.json`: rebuild the case before printing it for revision 2.
