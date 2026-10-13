# Carrier board

The board inside the cabinet that carries the ESP32-S3, drives the LED panel,
connects the microphone and shares out power. See [SPEC.md](SPEC.md).

Designed in KiCad 10. Placement is generated from `board.json`, which is the
single source for the outline, mounting holes and connector positions. The
casing CAD reads the same file.

## Rebuild the board placement

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
    electronics/carrier/tools/build_board.py
```

## Route

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
    electronics/carrier/tools/route_board.py
```

Needs Java and Freerouting 2.4.1 (`FREEROUTING_JAR`). It replaces all copper:
it draws the fixed tracks in `route_board.py`, autoroutes the rest, pours
ground and stitches it, and reports what is left unconnected. Rerun it after
every `build_board.py`.

## Check and preview

```sh
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
$K pcb drc --severity-all -o /tmp/carrier-drc.txt electronics/carrier/carrier.kicad_pcb
$K pcb render --side top -o /tmp/carrier-top.png electronics/carrier/carrier.kicad_pcb
```

## Design source

`tools/netlist.py` holds the circuit: every part, value, footprint and pin net.
Two generators read it:

- `tools/gen_schematic.py` writes `carrier.kicad_sch`. Any Python 3.9+ runs it.
- `tools/build_board.py` writes `carrier.kicad_pcb`, with every pad on its net,
  and `placement.json`. It needs KiCad's Python.

Do not edit the generated files by hand. Change `netlist.py`, `board.json` or
the fixed copper in `route_board.py` and regenerate:

```sh
python3 electronics/carrier/tools/gen_schematic.py
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
$KP electronics/carrier/tools/build_board.py
$KP electronics/carrier/tools/route_board.py
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
$K sch erc electronics/carrier/carrier.kicad_sch
$K pcb drc --schematic-parity electronics/carrier/carrier.kicad_pcb
```

## Status

Revision 1 is placed and routed.

- Schematic: passes the electrical rules check. The only warnings are the
  three intentional ties of unused buffer inputs to ground.
- Board: matches the schematic exactly, and passes the design-rule check with
  no errors and nothing unconnected. Four warnings remain: the silkscreen
  outlines of the USB-A socket (J4) and the LED power terminal (J6) overhang
  the board edge, as those connectors do by design.
- Chip resistor and capacitor references are left off the silkscreen (too
  dense); they remain on the fabrication layer.
- The USB-C power parts are marked do-not-populate. Their layout is complete
  but provisional; see SPEC.md before fitting them.

Library warnings in `kicad-cli` mean KiCad's global library tables are not set
up yet. Opening KiCad once creates them.
