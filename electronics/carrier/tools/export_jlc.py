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


def fitted():
    return [part for part in netlist.PARTS if not part.dnp and not part.ref.startswith(("H", "JP"))]


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
            x0, y0, x1, y1 = courtyards[part.ref]
            # KiCad's position files count Y upwards, as the Gerbers do.
            x, y = ORIGIN[0] + (x0 + x1) / 2, -(ORIGIN[1] + (y0 + y1) / 2)
            layer = "Top" if row["Side"] == "top" else "Bottom"
            writer.writerow([part.ref, f"{x:.3f}mm", f"{y:.3f}mm", layer, f"{float(row['Rot']):.0f}"])
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
