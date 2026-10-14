"""Writes the JLCPCB order files into electronics/carrier/fab/:
  carrier-gerbers.zip  Gerbers and drill files (upload as the PCB)
  carrier-bom.csv      parts with LCSC numbers (upload for assembly)
  carrier-cpl.csv      part positions and rotations (upload for assembly)

Run with any Python 3.9+ after route_board.py:
    python3 electronics/carrier/tools/export_jlc.py
Not-fitted parts (the USB-C power section, J8) are left out of both lists.
"""
import csv
import json
import math
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

import netlist

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "carrier.kicad_pcb"
OUT = ROOT / "fab"
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
ORIGIN = (50.0, 50.0)  # board corner on the KiCad page, as in build_board.py
LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"
# JLCPCB's part models sit at a different rotation from KiCad's footprints for
# some packages: degrees to add, by footprint name prefix (checked against
# JLCPCB's placement preview, 2026-10-14).
ROTATION_FIX = {"TSSOP-": -90, "SOT-23-": -90, "IDC-Header_": -90, "JST_GH_BM": 180}
# Parts placed from KiCad's origin rather than their courtyard (the S3
# module's courtyard includes its antenna keep-out), plus an offset in the
# footprint's own frame (mm, Y up, before rotation): JLCPCB centres the module
# on its pads, 3.77 mm from the body centre towards the pins.
ORIGIN_OFFSET = {"ESP32-S3-WROOM-1": (0.0, -3.77)}


def fitted():
    parts = [part for part in netlist.PARTS if not part.dnp and not part.ref.startswith(("H", "JP"))]
    usbc = [part.ref for part in parts if part.group == "usbc_power"]
    if usbc and not netlist.USBC_LAYOUT_REVIEWED:
        raise SystemExit(f"USB-C power parts are fitted ({', '.join(usbc)}) but the layout is not yet "
                         "fit to build. Finish SPEC.md's \"Before the USB-C build can be ordered\" list, "
                         "then set USBC_LAYOUT_REVIEWED in netlist.py.")
    return parts


def gerbers():
    with tempfile.TemporaryDirectory() as work:
        subprocess.run([KICAD_CLI, "pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask",
                        "--check-zones", "-o", work, str(BOARD)], check=True, capture_output=True)
        subprocess.run([KICAD_CLI, "pcb", "export", "drill", "--format", "excellon", "--excellon-units", "mm",
                        "--excellon-separate-th", "-o", work, str(BOARD)], check=True, capture_output=True)
        files = sorted(Path(work).iterdir())
        with zipfile.ZipFile(OUT / "carrier-gerbers.zip", "w", zipfile.ZIP_DEFLATED) as archive:
            for path in files:
                archive.write(path, path.name)
        return [path.name for path in files]


def bom(parts):
    """One line per LCSC part: JLCPCB's Comment, Designator, Footprint, LCSC columns."""
    lines = {}
    for part in parts:
        code = netlist.LCSC.get(part.ref)
        if not code:
            raise SystemExit(f"{part.ref}: no LCSC part number in netlist.LCSC")
        line = lines.setdefault(code, {"Comment": part.value, "Designator": [], "Footprint": part.footprint.split(":", 1)[1]})
        line["Designator"].append(part.ref)
    with open(OUT / "carrier-bom.csv", "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for code, line in lines.items():
            writer.writerow([line["Comment"], ",".join(line["Designator"]), line["Footprint"], code])
    return len(lines)


def cpl(parts):
    """Positions as JLCPCB wants them: the centre of each part (taken from its
    courtyard, which for connectors and the S3 module is nearer the body
    centre than KiCad's origin at pin 1), and KiCad's rotation."""
    with tempfile.TemporaryDirectory() as work:
        pos = Path(work) / "pos.csv"
        subprocess.run([KICAD_CLI, "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both",
                        "--exclude-dnp", "-o", str(pos), str(BOARD)], check=True, capture_output=True)
        rows = {row["Ref"]: row for row in csv.DictReader(pos.open())}
    courtyards = {item["ref"]: item["courtyard"] for item in json.loads((ROOT / "placement.json").read_text())["parts"]}
    with open(OUT / "carrier-cpl.csv", "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for part in parts:
            row = rows[part.ref]
            name = part.footprint.split(":", 1)[1]
            x0, y0, x1, y1 = courtyards[part.ref]
            # KiCad's position files count Y upwards, as the Gerbers do.
            x, y = ORIGIN[0] + (x0 + x1) / 2, -(ORIGIN[1] + (y0 + y1) / 2)
            for prefix, (dx, dy) in ORIGIN_OFFSET.items():
                if name.startswith(prefix):
                    turn = math.radians(float(row["Rot"]))
                    x = float(row["PosX"]) + dx * math.cos(turn) - dy * math.sin(turn)
                    y = float(row["PosY"]) + dx * math.sin(turn) + dy * math.cos(turn)
            rotation = float(row["Rot"]) + sum(fix for prefix, fix in ROTATION_FIX.items() if name.startswith(prefix))
            layer = "Top" if row["Side"] == "top" else "Bottom"
            writer.writerow([part.ref, f"{x:.3f}mm", f"{y:.3f}mm", layer, f"{rotation % 360:.0f}"])
    return len(parts)


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir()
    parts = fitted()
    files = gerbers()
    lines = bom(parts)
    placed = cpl(parts)
    print(f"Wrote fab/: {len(files)} Gerber and drill files, {lines} BOM lines, {placed} placed parts")


if __name__ == "__main__":
    main()
