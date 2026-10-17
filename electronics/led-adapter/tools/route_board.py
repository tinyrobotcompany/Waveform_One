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
base.POWER_PATHS = base.J3_TRACKS = base.J2_TRACKS = base.SMALL_TRACKS = []
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
