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

## Status

Revision 1, placement only: the outline, four M3 holes and every connector are
positioned. The schematic and routing come next. Until nets exist, DRC reports
the USB-C pads that will share a net as touching; that is expected.
