"""Routes carrier.kicad_pcb: fixed copper, then Freerouting, then ground
pours and stitching vias.

Run after build_board.py, with KiCad's Python:
    .../Python.framework/Versions/Current/bin/python3 electronics/carrier/tools/route_board.py
Needs Java and Freerouting 2.4.1 (FREEROUTING_JAR; default: the scratch folder
this was developed with).

1. Net classes: signals 0.25 mm; supply nets 0.5 mm.
2. Locked copper drawn here, where the autorouter cannot do the job:
   - the 6 A path as wide tracks: barrel socket to fuse (2.0 mm), fuse to the
     bulk capacitor and on to the LED panel terminal (2.5 mm), the 2 A P4
     feed (1.5 mm);
   - the S3's USB-C socket escapes (interleaved D+/D-, CC lines, the paired
     VBUS pads), and USB D- on to the module;
   - links between pins on one net that the autorouter treats as joined
     (USBLC6 flow-through, buffer DIR to VCC, the AP2112's VIN to EN);
   - the USB-C power section's VBUS and the buck converter's switch node.
3. Freerouting routes everything else and the session is imported.
4. Both layers get a ground pour, then ground vias are stitched into empty
   spaces and the pours refilled.
5. KiCad's design-rule check counts what is still unconnected. Freerouting is
   not deterministic, so a failed attempt is retried (ATTEMPTS).
Each stage runs in its own process: KiCad's Python bindings misbehave once a
session has been imported.
"""
import json
import math
import os
import subprocess
import sys
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "carrier.kicad_pcb"
ORIGIN = (50.0, 50.0)
JAR = Path(os.environ.get("FREEROUTING_JAR", "/private/tmp/claude-501/-Users-simonholmes-Projects-Applications-Waveform-One/"
                          "d0efb211-acb3-4226-99b0-43a27c99b055/scratchpad/freerouting/freerouting-2.4.1.jar"))
JAVA = os.environ.get("JAVA", "/opt/homebrew/opt/openjdk/bin/java")

SUPPLY_NETS = ["+5V", "+3V3", "GND", "P4_5V", "VBUS_PD", "BUCK_SW"]

# (net, width mm, [(x, y), ...] in board coordinates), all on the top layer.
POWER_PATHS = [
    ("VIN_BARREL", 2.0, [(24.0, 6.1), (24.0, 10.0), (25.39, 11.4), (25.39, 16.0)]),
    ("+5V", 2.5, [(18.61, 16.0), (18.61, 26.5), (29.8, 26.5), (29.8, 30.0)]),
    ("+5V", 2.5, [(29.8, 30.0), (29.8, 57.5), (76.0, 57.5), (76.0, 68.1)]),
    ("+5V", 1.5, [(29.8, 46.14), (20.0, 46.14)]),
    ("P4_5V", 1.5, [(20.0, 41.86), (8.0, 41.86), (8.0, 48.0), (1.4, 48.0)]),
]


# The S3's USB-C socket (J3): its D+ and D- pins interleave at 0.5 mm, so one
# of each pair crosses on the bottom layer. Hand-placed escapes, clearances
# >= 0.2 mm: (net, layer, width, points) tracks and (net, x, y) vias.
VBUS_J3 = "unconnected-(J3-VBUS-PadA4)"
J3_TRACKS = [
    ("USB_DP", "F", 0.2, [(1.17, 23.25), (1.9, 23.25), (2.6, 22.75)]),          # B6 to its via
    ("USB_DP", "B", 0.2, [(2.6, 22.75), (5.2, 24.25), (5.2, 24.85)]),          # bridge under the row
    ("USB_DP", "F", 0.2, [(1.17, 24.25), (4.6, 24.25), (5.2, 24.85)]),          # A6 to the bridge via
    ("USB_DM", "F", 0.2, [(1.17, 24.75), (1.9, 24.75), (3.4, 25.2)]),           # B7 to its via
    ("USB_DM", "B", 0.2, [(3.4, 25.2), (3.4, 26.2), (6.4, 26.2), (6.4, 23.75)]),  # bridge below
    ("USB_DM", "F", 0.2, [(1.17, 23.75), (6.4, 23.75)]),                        # A7 to the bridge via
    # CC lines run on the top layer to their 5.1k resistors (R4 above, R3 below).
    ("S3_CC2", "F", 0.15, [(1.17, 22.25), (1.75, 22.25), (1.9, 22.12), (8.5, 22.12), (11.17, 19.5)]),
    ("S3_CC1", "F", 0.15, [(1.17, 25.25), (1.9, 25.25), (2.5, 25.85), (8.0, 25.85), (11.17, 29.0)]),
    # The two unused VBUS pads (one net in the symbol) joined by a thin bottom
    # layer loop around the crossings; it carries nothing.
    (VBUS_J3, "F", 0.15, [(1.6, 21.75), (3.2, 21.75), (3.8, 21.15)]),
    (VBUS_J3, "F", 0.15, [(1.6, 26.25), (3.2, 26.25), (3.8, 26.85)]),
    (VBUS_J3, "B", 0.15, [(3.8, 21.15), (7.2, 21.15), (7.2, 26.85), (3.8, 26.85)]),
]
J3_VIAS = [("USB_DP", 2.6, 22.75), ("USB_DP", 5.2, 24.85), ("USB_DM", 3.4, 25.2), ("USB_DM", 6.4, 23.75),
           (VBUS_J3, 3.8, 21.15), (VBUS_J3, 3.8, 26.85)]
# The USB-C power socket (J2, not fitted): join its two VBUS pads inside the
# locating-peg holes. 0.25 mm: widen (or add a pour) before fitting USB-C power.
J2_TRACKS = [("VBUS_PD", "F", 0.25, [(43.75, 1.33), (43.75, 4.2), (48.25, 4.2), (48.25, 1.33)]),
              # ...and down to the PD controller's VBUS pin (U2 pin 1).
              ("VBUS_PD", "F", 0.8, [(44.5, 4.2), (44.5, 10.0), (43.36, 11.14), (43.36, 12.5)]),
              # ...and over U2 to the buck converter's input feed.
              ("VBUS_PD", "F", 0.8, [(44.5, 10.0), (54.0, 10.0), (54.0, 14.0)]),
              # U2's other VBUS pin (8) to the input capacitor C10.
              ("VBUS_PD", "F", 0.5, [(48.64, 14.5), (49.66, 14.5), (51.1, 15.95), (51.5, 15.95)])]
SMALL_TRACKS = [
    # The CH224A's VBUS sense divider (R13) to the VBUS feed.
    ("VBUS_PD", "F", 0.5, [(57.15, 14.0), (58.22, 14.0), (61.0, 11.225)]),
    # C3's +5V pad to the 5 V trunk just below it.
    ("+5V", "F", 0.5, [(24.72, 24.5), (24.72, 26.0)]),
    # U6: DIR (pin 1) to VCC (pin 20), straight under the package.
    ("+5V", "F", 0.3, [(41.08, 42.36), (41.08, 36.64)]),
    # U3 (rotated so its SW pins face L1): its two VIN pins, joined between
    # the SW pads and the left pin column.
    ("VBUS_PD", "F", 0.25, [(57.15, 18.9), (56.5, 18.7), (56.5, 17.3), (57.15, 17.1)]),
    # U3's switch node to L1: short and wide (up to 8 A).
    ("BUCK_SW", "F", 1.0, [(58.6, 18.0), (59.6, 18.0)]),
    ("BUCK_SW", "F", 1.5, [(59.6, 18.0), (63.6, 18.0), (65.6, 16.0), (66.09, 16.0)]),
    # The boot capacitor's SW side down from the switch-node track.
    ("BUCK_SW", "F", 0.5, [(59.17, 21.8), (59.9, 21.07), (59.9, 18.6)]),
    # U3's ground pins 3-5 are boxed in by the SW tracks: own ground via.
    ("GND", "F", 0.25, [(58.35, 19.38), (58.35, 20.6)]),
    # VIN pin 11 out between pins 12 and 10, on to the input capacitor C10.
    ("VBUS_PD", "F", 0.45, [(57.15, 16.62), (57.15, 14.0)]),
    ("VBUS_PD", "F", 0.8, [(57.15, 14.0), (54.0, 14.0), (52.6, 15.95), (51.5, 15.95)]),
    # J3's escapes on to U4: D- joins the under-body link, D+ passes below
    # U4's pins 1 and 2 to reach pin 3.
    ("USB_DM", "F", 0.25, [(6.4, 23.75), (13.05, 23.75)]),
    ("USB_DP", "F", 0.25, [(5.2, 24.85), (11.0, 24.85), (12.3, 26.15), (14.6, 26.15), (14.95, 25.8)]),
    # U4 to the S3 module: D- under the 5 V trunk on the bottom layer
    # (Freerouting finds D+ but not this one).
    ("USB_DM", "F", 0.25, [(13.05, 22.86), (13.05, 21.3)]),
    ("USB_DM", "B", 0.25, [(13.05, 21.3), (12.4, 21.95), (12.4, 27.3), (66.8, 27.3)]),
    ("USB_DM", "F", 0.25, [(66.8, 27.3), (76.02, 27.3), (76.02, 29.25)]),
    # U4 (USBLC6): flow-through, each data line straight under the body.
    ("USB_DM", "F", 0.25, [(13.05, 22.86), (13.05, 25.14)]),
    ("USB_DP", "F", 0.25, [(14.95, 22.86), (14.95, 25.14)]),
    # U5 (AP2112): VIN (pin 1) round to EN (pin 3) via C7's pad.
    ("+5V", "F", 0.5, [(65.86, 32.05), (65.2, 32.05), (64.6, 32.65), (64.6, 33.78), (63.5, 33.78)]),
    ("GND", "F", 0.25, [(14.0, 24.0), (14.0, 25.14)]),
    # U4's VBUS pin (5 V) up to a via, then under to the 5 V trunk.
    ("+5V", "F", 0.3, [(14.0, 22.86), (14.0, 21.5)]),
    ("+5V", "B", 0.5, [(14.0, 21.5), (14.0, 17.0), (18.61, 17.0)]),   # clear of the USB pair's path
    # U6's supply from the 5 V trunk, on the bottom layer.
    ("+5V", "F", 0.3, [(41.08, 36.64), (41.08, 35.7), (40.4, 35.0)]),
    ("+5V", "B", 0.5, [(40.4, 35.0), (29.8, 35.0)]),
    # U3 pin 12 (GND) is boxed in by the VIN link: own ground via.
    ("GND", "F", 0.25, [(56.58, 16.35), (56.3, 15.3)]),
]
SMALL_VIAS = [("+5V", 40.4, 35.0), ("+5V", 29.8, 35.0), ("GND", 56.3, 15.3), ("GND", 58.35, 20.6),
              ("+5V", 14.0, 21.5), ("+5V", 18.61, 17.0), ("USB_DM", 13.05, 21.3), ("USB_DM", 66.8, 27.3)]
# U4's GND pin sits between its two flow-through links: a small via (0.5 mm
# pad, 0.25 mm drill) between the pin rows takes it to the bottom pour.
TIGHT_VIAS = [("GND", 14.0, 24.0, 0.5, 0.25)]


def mm(x, y):
    return pcbnew.VECTOR2I_MM(ORIGIN[0] + x, ORIGIN[1] + y)


def net_classes(board):
    settings = board.GetDesignSettings().m_NetSettings
    default = settings.GetDefaultNetclass()
    default.SetTrackWidth(pcbnew.FromMM(0.25))
    default.SetClearance(pcbnew.FromMM(0.2))
    default.SetViaDiameter(pcbnew.FromMM(0.6))
    default.SetViaDrill(pcbnew.FromMM(0.3))
    supply = pcbnew.NETCLASS("Supply")
    supply.SetTrackWidth(pcbnew.FromMM(0.5))
    supply.SetClearance(pcbnew.FromMM(0.2))
    supply.SetViaDiameter(pcbnew.FromMM(0.8))
    supply.SetViaDrill(pcbnew.FromMM(0.4))
    settings.SetNetclass("Supply", supply)
    settings.ClearNetclassPatternAssignments()
    for name in SUPPLY_NETS:
        settings.SetNetclassPatternAssignment("/" + name, "Supply")
    settings.RecomputeEffectiveNetclasses()
    board.SynchronizeNetsAndNetClasses(True)


def clear_copper(board):
    for item in list(board.GetTracks()):
        board.Remove(item)
    for zone in list(board.Zones()):
        board.Remove(zone)


def power_paths(board):
    for name, width, points in POWER_PATHS:
        net = board.FindNet("/" + name)
        if net is None:
            raise SystemExit(f"No net {name}")
        for a, b in zip(points, points[1:]):
            track = pcbnew.PCB_TRACK(board)
            track.SetStart(mm(*a)); track.SetEnd(mm(*b))
            track.SetWidth(pcbnew.FromMM(width))
            track.SetLayer(pcbnew.F_Cu)
            track.SetNet(net)
            track.SetLocked(True)
            board.Add(track)


def find_net(board, name):
    net = board.FindNet(name if name.startswith("unconnected-") else "/" + name)
    if net is None:
        raise SystemExit(f"No net {name}")
    return net


def fixed_escapes(board):
    layers = {"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}
    for name, layer, width, points in J3_TRACKS + J2_TRACKS + SMALL_TRACKS:
        net = find_net(board, name)
        for a, b in zip(points, points[1:]):
            track = pcbnew.PCB_TRACK(board)
            track.SetStart(mm(*a)); track.SetEnd(mm(*b))
            track.SetWidth(pcbnew.FromMM(width))
            track.SetLayer(layers[layer])
            track.SetNet(net)
            track.SetLocked(True)
            board.Add(track)
    for name, x, y, size, drill in [via + (0.6, 0.3) for via in J3_VIAS + SMALL_VIAS] + TIGHT_VIAS:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(mm(x, y))
        via.SetWidth(pcbnew.FromMM(size))
        via.SetDrill(pcbnew.FromMM(drill))
        via.SetNet(find_net(board, name))
        via.SetLocked(True)
        board.Add(via)


def _segment_distance(point, a, b):
    (px, py), (ax, ay), (bx, by) = point, a, b
    dx, dy = bx - ax, by - ay
    length = dx * dx + dy * dy
    t = 0.0 if length == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def routed_copper():
    """[(width, [a, b])] track segments and [(x, y)] vias, in board coordinates,
    read from the saved board (the pcbnew bindings are unreliable after an
    import)."""
    sys.path.insert(0, str(Path(__file__).parent))
    import kicad_lib
    tree = kicad_lib.parse(BOARD.read_text())
    def xy(node):
        return float(node[1]) - ORIGIN[0], float(node[2]) - ORIGIN[1]
    segments, vias = [], []
    for item in kicad_lib.find(tree, "segment"):
        width = float(kicad_lib.find(item, "width")[0][1])
        segments.append((width, [xy(kicad_lib.find(item, "start")[0]), xy(kicad_lib.find(item, "end")[0])]))
    for item in kicad_lib.find(tree, "via"):
        vias.append(xy(kicad_lib.find(item, "at")[0]))
    return segments, vias


def stitching_sites():
    """Ground via positions on a 5 mm grid wherever the routed board is empty:
    clear of part courtyards, mounting holes, tracks, vias and the antenna
    keep-out. Plain geometry, from the saved files."""
    geometry = json.loads((ROOT / "board.json").read_text())["board"]
    placement = json.loads((ROOT / "placement.json").read_text())
    boxes = [part["courtyard"] for part in placement["parts"] if part.get("courtyard")]
    holes = [tuple(hole) for hole in placement.get("mounting_holes", [])]
    copper, vias = routed_copper()
    sites = []
    y = 4.0
    while y < geometry["depth"] - 3:
        x = 4.0
        while x < geometry["width"] - 3:
            clear = x < 90.0
            clear = clear and not any(x0 - 0.5 <= x <= x1 + 0.5 and y0 - 0.5 <= y <= y1 + 0.5 for x0, y0, x1, y1 in boxes)
            clear = clear and all(math.hypot(x - hx, y - hy) > 4.0 for hx, hy in holes)
            clear = clear and all(math.hypot(x - vx, y - vy) > 1.0 for vx, vy in vias)
            clear = clear and all(_segment_distance((x, y), a, b) > width / 2 + 0.6
                                  for width, points in copper for a, b in zip(points, points[1:]))
            if clear:
                sites.append((x, y))
            x += 5.0
        y += 5.0
    return sites


def stitch():
    """Second pass, in a fresh process: ground vias into the routed board's
    empty spaces, then refill the pours."""
    sites = stitching_sites()
    board = pcbnew.LoadBoard(str(BOARD))
    vias = stitching_vias(board, sites)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    # A site can land on a pour island the tracks have cut off; drop those.
    zones = [zone for zone in board.Zones() if zone.GetNetname() == "/GND"]
    stranded = [via for via in vias if not all(
        any(zone.HitTestFilledArea(layer, via.GetPosition()) for zone in zones) for layer in (pcbnew.F_Cu, pcbnew.B_Cu))]
    for via in stranded:
        board.Remove(via)
    if stranded:
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(str(BOARD))
    print(f"{len(vias) - len(stranded)} stitching vias")


def stitching_vias(board, sites):
    net = board.FindNet("/GND")
    vias = []
    for x, y in sites:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(mm(x, y))
        via.SetWidth(pcbnew.FromMM(0.6))
        via.SetDrill(pcbnew.FromMM(0.3))
        via.SetNet(net)
        via.SetLocked(True)
        board.Add(via)
        vias.append(via)
    return vias


def autoroute(board, work):
    dsn, ses = work / "carrier.dsn", work / "carrier.ses"
    if not pcbnew.ExportSpecctraDSN(board, str(dsn)):
        raise SystemExit("DSN export failed")
    result = subprocess.run([JAVA, "-jar", str(JAR), "-de", str(dsn), "-do", str(ses), "-mp", "30",
                             "--gui.enabled=false"], capture_output=True, text=True, timeout=1800)
    log = (result.stdout + result.stderr).strip().splitlines()
    print("\n".join(line for line in log if "unrouted" in line.lower() or "completed" in line.lower() or "error" in line.lower())[-2000:])
    if not ses.exists():
        raise SystemExit("Freerouting produced no session file")
    if not pcbnew.ImportSpecctraSES(board, str(ses)):
        raise SystemExit("SES import failed")


def ground_pours(board):
    # The board's rectangle from board.json (the fill keeps its own edge
    # clearance and follows the rounded corners).
    geometry = json.loads((ROOT / "board.json").read_text())["board"]
    corners = [(0, 0), (geometry["width"], 0), (geometry["width"], geometry["depth"]), (0, geometry["depth"])]
    net = board.FindNet("/GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        zone = pcbnew.ZONE(board)
        zone.SetLayer(layer)
        zone.SetNet(net)
        for corner in corners:
            zone.AppendCorner(mm(*corner), -1)
        zone.SetLocalClearance(pcbnew.FromMM(0.3))
        zone.SetMinThickness(pcbnew.FromMM(0.25))
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # SMD pads solid
        zone.SetThermalReliefGap(pcbnew.FromMM(0.3))
        zone.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.5))
        zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        board.Add(zone)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


KICAD_CLI = os.environ.get("KICAD_CLI", "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
ATTEMPTS = 6


def unconnected(work):
    """KiCad's own count of unrouted connections in the saved board."""
    report = work / "drc.json"
    subprocess.run([KICAD_CLI, "pcb", "drc", "--format", "json", "-o", str(report), str(BOARD)],
                   capture_output=True, text=True)
    return len(json.loads(report.read_text()).get("unconnected_items", []))


def route_once(work, reuse=False):
    board = pcbnew.LoadBoard(str(BOARD))
    net_classes(board)
    clear_copper(board)
    power_paths(board)
    fixed_escapes(board)
    if reuse:
        pcbnew.ImportSpecctraSES(board, str(work / "carrier.ses"))
    else:
        autoroute(board, work)
    ground_pours(board)
    board.Save(str(BOARD))


def main():
    work = ROOT / "build"
    work.mkdir(exist_ok=True)
    if os.environ.get("REUSE_SES") and (work / "carrier.ses").exists():
        route_once(work, reuse=True)
        print(f"Routed {BOARD.name} from the saved session: {unconnected(work)} unconnected")
        return
    if os.environ.get("ROUTE_ONCE"):
        route_once(work)
        return
    if os.environ.get("STITCH"):
        stitch()
        return
    # Freerouting is not deterministic; keep the first fully connected result.
    # Each attempt runs in a fresh process: KiCad's bindings misbehave once a
    # session has been imported.
    for attempt in range(1, ATTEMPTS + 1):
        result = subprocess.run([sys.executable, __file__], env={**os.environ, "ROUTE_ONCE": "1"},
                                capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        result = subprocess.run([sys.executable, __file__], env={**os.environ, "STITCH": "1"},
                                capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        missing = unconnected(work)
        print(f"Attempt {attempt}: {missing} unconnected")
        if missing == 0:
            break
    print(f"Routed {BOARD.name}")


if __name__ == "__main__":
    main()
