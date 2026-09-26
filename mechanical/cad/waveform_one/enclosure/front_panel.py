"""M3 front-layout mock-up: a flat front panel split into three printable pieces.

The panel occupies the front-bezel slab (Y 0..FRONT_BEZEL_THICKNESS). Windows
reveal the display's visible area and the LED matrix. Seams are vertical and
sit in the blank bands beside the windows, so no seam crosses a window.

Every piece prints face-down (front face on the bed): no supports, and the
visible face gets the bed's surface finish.
"""

from build123d import Axis, Location, Part, Rot, chamfer

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box, box_between

FACE_DOWN = Location(Rot(90, 0, 0))
FACE_DOWN_NOTE = "Front face down on the bed; no supports."


def seam_x() -> float:
    """Distance of each vertical seam from the centre line."""
    widest = max(p.DISPLAY_W.v, p.MATRIX_W.v)
    return widest / 2 + p.FRONT_SEAM_MARGIN.v


def display_window() -> tuple[tuple[float, float], tuple[float, float]]:
    """((x_min, x_max), (z_min, z_max)) of the display window."""
    x, _, z = layout.display_origin()
    margin = p.DISPLAY_WINDOW_MARGIN.v
    cx = x + p.DISPLAY_W.v / 2 + p.DISPLAY_ACTIVE_OFFSET_X.v
    cz = z + p.DISPLAY_H.v / 2 + p.DISPLAY_ACTIVE_OFFSET_Z.v
    half_w, half_h = p.DISPLAY_ACTIVE_W.v / 2 + margin, p.DISPLAY_ACTIVE_H.v / 2 + margin
    return (cx - half_w, cx + half_w), (cz - half_h, cz + half_h)


def matrix_window() -> tuple[tuple[float, float], tuple[float, float]]:
    x, _, z = layout.matrix_origin()
    lip = p.MATRIX_WINDOW_LIP.v
    return (x + lip, x + p.MATRIX_W.v - lip), (z + lip, z + p.MATRIX_H.v - lip)


def build_full_panel() -> Part:
    thickness = p.FRONT_BEZEL_THICKNESS.v
    half = p.CASE_WIDTH.v / 2
    panel = box((-half, 0, 0), (p.CASE_WIDTH.v, thickness, p.CASE_HEIGHT.v))
    for (x0, x1), (z0, z1) in (display_window(), matrix_window()):
        panel -= box_between((x0, -thickness, z0), (x1, 2 * thickness, z1))
    front = panel.faces().sort_by(Axis.Y)[0]
    return chamfer(front.inner_wires().edges(), p.WINDOW_CHAMFER.v)


def build_pieces() -> list[PrintedPart]:
    panel = build_full_panel()
    half, s = p.CASE_WIDTH.v / 2, seam_x()
    thickness, height = p.FRONT_BEZEL_THICKNESS.v, p.CASE_HEIGHT.v

    def slice_x(x0: float, x1: float) -> Part:
        o = CUT_OVERSHOOT
        return panel & box_between((x0, -o, -o), (x1, thickness + o, height + o))

    return [
        PrintedPart("front_left", slice_x(-half, -s), FACE_DOWN, FACE_DOWN_NOTE),
        PrintedPart("front_center", slice_x(-s, s), FACE_DOWN, FACE_DOWN_NOTE),
        PrintedPart("front_right", slice_x(s, half), FACE_DOWN, FACE_DOWN_NOTE),
    ]


def build_splices() -> list[PrintedPart]:
    """Plates glued across the back of each seam."""
    w, t = p.SPLICE_WIDTH.v, p.SPLICE_THICKNESS.v
    z0, z1 = p.SPLICE_END_MARGIN.v, p.CASE_HEIGHT.v - p.SPLICE_END_MARGIN.v
    y = p.FRONT_BEZEL_THICKNESS.v
    return [
        PrintedPart(
            f"front_splice_{side}",
            box((sign * seam_x() - w / 2, y, z0), (w, t, z1 - z0)),
            FACE_DOWN,
            "Flat; either face down.",
        )
        for side, sign in (("left", -1), ("right", 1))
    ]


def build_stands() -> list[PrintedPart]:
    """Slotted feet that hold the panel upright, each straddling a seam."""
    t = p.FRONT_BEZEL_THICKNESS.v
    slot_w = t + p.STAND_SLOT_CLEARANCE.v
    base_z = p.STAND_SLOT_DEPTH.v - p.STAND_HEIGHT.v  # panel bottom rests at Z=0 in the slot
    y0 = -p.STAND_FRONT.v - p.STAND_SLOT_CLEARANCE.v / 2
    length = p.STAND_FRONT.v + slot_w + p.STAND_REAR.v
    stands = []
    for side, sign in (("left", -1), ("right", 1)):
        x0 = sign * seam_x() - p.STAND_WIDTH.v / 2
        stand = box((x0, y0, base_z), (p.STAND_WIDTH.v, length, p.STAND_HEIGHT.v))
        o = CUT_OVERSHOOT
        stand -= box((x0 - o, -p.STAND_SLOT_CLEARANCE.v / 2, 0), (p.STAND_WIDTH.v + 2 * o, slot_w, p.STAND_HEIGHT.v))
        stands.append(PrintedPart(f"stand_{side}", stand, Location(), "Base down, slot up; no supports.", mockup_only=True))
    return stands


def build_all() -> list[PrintedPart]:
    return build_pieces() + build_splices() + build_stands()
