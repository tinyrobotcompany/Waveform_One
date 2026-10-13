"""Builds carrier.kicad_pcb's outline, mounting holes and part placement from board.json.

Run with KiCad's own Python (it provides pcbnew):
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
      electronics/carrier/tools/build_board.py
Routing and nets come later from the schematic; this fixes where things go so
the casing can be designed around them.
"""
import json
import math
import os
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = Path(os.environ.get("KICAD_FOOTPRINTS",
    "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"))
ORIGIN = (50.0, 50.0)  # board corner on the KiCad page


def mm(x, y):
    return pcbnew.VECTOR2I_MM(ORIGIN[0] + x, ORIGIN[1] + y)


def outline(board, width, depth, radius):
    """Rounded rectangle on Edge.Cuts."""
    def line(a, b):
        shape = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        shape.SetStart(mm(*a)); shape.SetEnd(mm(*b))
        shape.SetLayer(pcbnew.Edge_Cuts); shape.SetWidth(pcbnew.FromMM(0.1))
        board.Add(shape)

    def arc(centre, start_angle):
        # Quarter circle from start_angle, counter-clockwise on screen.
        start = (centre[0] + radius * math.cos(math.radians(start_angle)),
                 centre[1] + radius * math.sin(math.radians(start_angle)))
        middle = (centre[0] + radius * math.cos(math.radians(start_angle + 45)),
                  centre[1] + radius * math.sin(math.radians(start_angle + 45)))
        end = (centre[0] + radius * math.cos(math.radians(start_angle + 90)),
               centre[1] + radius * math.sin(math.radians(start_angle + 90)))
        shape = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        shape.SetArcGeometry(mm(*start), mm(*middle), mm(*end))
        shape.SetLayer(pcbnew.Edge_Cuts); shape.SetWidth(pcbnew.FromMM(0.1))
        board.Add(shape)

    r, w, d = radius, width, depth
    line((r, 0), (w - r, 0)); line((w, r), (w, d - r))
    line((w - r, d), (r, d)); line((0, d - r), (0, r))
    arc((w - r, r), 270); arc((w - r, d - r), 0); arc((r, d - r), 90); arc((r, r), 180)


EDGE_CLEARANCE = 0.6  # copper to board edge, a little over the 0.5 mm rule


def keep_pads_inside(part, edge, width, depth):
    """Moves an edge connector inwards until all its copper clears the edge.
    The body still overhangs, so the socket reaches the case opening."""
    pads = [pad.GetBoundingBox() for pad in part.Pads()]
    if not pads or edge not in ("rear", "front", "left", "right"):
        return
    to_mm = pcbnew.ToMM
    left = min(to_mm(b.GetLeft()) for b in pads) - ORIGIN[0]
    right = max(to_mm(b.GetRight()) for b in pads) - ORIGIN[0]
    top = min(to_mm(b.GetTop()) for b in pads) - ORIGIN[1]
    bottom = max(to_mm(b.GetBottom()) for b in pads) - ORIGIN[1]
    dx = dy = 0.0
    if edge == "rear" and top < EDGE_CLEARANCE: dy = EDGE_CLEARANCE - top
    if edge == "front" and bottom > depth - EDGE_CLEARANCE: dy = depth - EDGE_CLEARANCE - bottom
    if edge == "left" and left < EDGE_CLEARANCE: dx = EDGE_CLEARANCE - left
    if edge == "right" and right > width - EDGE_CLEARANCE: dx = width - EDGE_CLEARANCE - right
    if dx or dy:
        part.Move(pcbnew.VECTOR2I_MM(dx, dy))


def place(board, reference, footprint, x, y, rotation, value=""):
    library, name = footprint.split(":", 1)
    part = pcbnew.FootprintLoad(str(LIBRARIES / f"{library}.pretty"), name)
    if part is None:
        raise SystemExit(f"Footprint not found: {footprint}")
    part.SetReference(reference)
    part.SetValue(value or name)
    part.SetPosition(mm(x, y))
    part.SetOrientationDegrees(rotation)
    board.Add(part)
    return part


def main():
    spec = json.loads((ROOT / "board.json").read_text())
    geometry = spec["board"]
    board = pcbnew.BOARD()
    settings = board.GetDesignSettings()
    settings.SetBoardThickness(pcbnew.FromMM(geometry["thickness"]))
    # Standard 2-layer process limits (JLCPCB/PCBWay class); recheck against
    # the chosen maker before ordering.
    settings.m_MinThroughDrill = pcbnew.FromMM(0.2)
    settings.m_HoleClearance = pcbnew.FromMM(0.15)
    settings.m_CopperEdgeClearance = pcbnew.FromMM(0.5)
    outline(board, geometry["width"], geometry["depth"], geometry["corner_radius"])
    holes = spec["mounting_holes"]
    for index, (x, y) in enumerate(holes["positions"], start=1):
        hole = place(board, f"H{index}", holes["footprint"], x, y, 0, "M3 to heat-set insert")
        hole.Reference().SetVisible(False)
    for part in spec["parts"]:
        placed = place(board, part["ref"], part["footprint"], part["x"], part["y"], part["rotation"], part["role"])
        keep_pads_inside(placed, part.get("edge"), geometry["width"], geometry["depth"])
    output = ROOT / "carrier.kicad_pcb"
    board.Save(str(output))
    print(f"Wrote {output.relative_to(ROOT.parents[1])}")
    write_placement(board, spec)


def write_placement(board, spec):
    """Where every part actually ended up (after edge nudges), for the casing.
    Courtyards are in board coordinates: x right, y towards the front."""
    to_mm = pcbnew.ToMM
    parts = []
    by_ref = {part["ref"]: part for part in spec["parts"]}
    for footprint in board.GetFootprints():
        ref = footprint.GetReference()
        if ref not in by_ref:
            continue
        courtyard = footprint.GetCourtyard(pcbnew.F_CrtYd).BBox()
        if courtyard.GetWidth() == 0:
            courtyard = footprint.GetBoundingBox(False)
        parts.append({
            "ref": ref,
            "role": by_ref[ref]["role"],
            "edge": by_ref[ref]["edge"],
            "height": by_ref[ref]["height"],
            "courtyard": [round(to_mm(courtyard.GetLeft()) - ORIGIN[0], 3), round(to_mm(courtyard.GetTop()) - ORIGIN[1], 3),
                          round(to_mm(courtyard.GetRight()) - ORIGIN[0], 3), round(to_mm(courtyard.GetBottom()) - ORIGIN[1], 3)],
        })
    parts.sort(key=lambda part: part["ref"])
    output = ROOT / "placement.json"
    output.write_text(json.dumps({
        "_comment": "Generated by tools/build_board.py from board.json and the KiCad footprints; do not edit.",
        "board": spec["board"],
        "mounting_holes": spec["mounting_holes"]["positions"],
        "parts": parts,
    }, indent=2) + "\n")
    print(f"Wrote {output.relative_to(ROOT.parents[1])}")


if __name__ == "__main__":
    main()
