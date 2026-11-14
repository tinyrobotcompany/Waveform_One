"""Routes led-adapter.kicad_pcb with the carrier's router: Freerouting, then
ground pours and stitching vias. Nothing here needs hand-drawn copper.

Run after build_board.py, with KiCad's Python (needs Java and Freerouting,
as for the carrier):
    .../Python.framework/Versions/Current/bin/python3 electronics/led-adapter/tools/route_board.py
"""
import os
import subprocess
import sys

from reuse import ROOT, carrier, netlist

base = carrier("route_board")
base.SUPPLY_NETS = netlist.SUPPLY_NETS
# Wide top-layer copper for the heavy currents, in board coordinates: the
# barrel socket J5 to the LED panel's terminal J4 (up to 4 A), with a branch
# to the P4's fuse F1 (up to 2 A). The router does everything else.
base.POWER_PATHS = [
    ("+5V", 2.5, [(15.0, 12.3), (26.0, 12.3), (26.0, 33.3), (39.0, 33.3), (39.0, 36.5)]),
    ("+5V", 1.5, [(26.0, 33.3), (13.0, 33.3), (13.0, 32.14)]),
]
# Hand-placed tracks where the router can't find room, clearances >= 0.2 mm:
# the USB-C socket's two 5 V pads leave towards the board's middle and join
# at the fuse; short +5V stubs to C3, the buffers' C1 and the LED's resistor.
base.SMALL_TRACKS = [
    ("P4_5V", "F", 0.5, [(7.355, 27.6), (8.6, 27.6)]),
    ("P4_5V", "F", 0.6, [(8.6, 27.6), (13.0, 27.6)]),
    ("P4_5V", "F", 0.5, [(7.355, 32.4), (8.6, 32.4)]),
    ("P4_5V", "F", 0.6, [(8.6, 32.4), (10.4, 32.4), (10.4, 27.6)]),
    ("+5V", "F", 0.5, [(26.0, 27.0), (32.53, 27.0)]),
    ("+5V", "F", 0.5, [(26.0, 25.5), (45.225, 25.5)]),
    ("+5V", "F", 0.4, [(43.08, 22.86), (43.08, 25.5)]),
    ("+5V", "F", 0.4, [(21.175, 38.0), (21.175, 33.3)]),
]
base.J3_TRACKS = base.J2_TRACKS = []
base.J3_VIAS = base.SMALL_VIAS = base.TIGHT_VIAS = []


def main():
    work = ROOT / "build"
    work.mkdir(exist_ok=True)
    if os.environ.get("ROUTE_ONCE"):
        base.route_once(work)
        return
    if os.environ.get("STITCH"):
        base.stitch()
        return
    # As for the carrier: each stage in a fresh process; keep the first fully
    # connected result.
    for attempt in range(1, base.ATTEMPTS + 1):
        for stage in ("ROUTE_ONCE", "STITCH"):
            result = subprocess.run([sys.executable, __file__], env={**os.environ, stage: "1"},
                                    capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(result.stdout + result.stderr)
        missing = base.unconnected(work)
        print(f"Attempt {attempt}: {missing} unconnected")
        if missing == 0:
            break
    print(f"Routed {base.BOARD.name}")


if __name__ == "__main__":
    main()
