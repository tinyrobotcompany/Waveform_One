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

Do not edit the generated files by hand. Change `netlist.py` or `board.json`
and regenerate:

```sh
python3 electronics/carrier/tools/gen_schematic.py
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
    electronics/carrier/tools/build_board.py
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
$K sch erc electronics/carrier/carrier.kicad_sch
$K pcb drc --schematic-parity electronics/carrier/carrier.kicad_pcb
```

## Status

Revision 1: the schematic is complete and passes the electrical rules check.
The only warnings are the three intentional ties of unused buffer inputs to
ground. The board matches the schematic exactly. The connectors, module and
holes are placed; the passives wait beside the board for layout, and routing
comes next. The USB-C power parts are marked do-not-populate.

Library warnings in `kicad-cli` mean KiCad's global library tables are not set
up yet. Opening KiCad once creates them.
