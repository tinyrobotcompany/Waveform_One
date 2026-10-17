"""Removable rear panel: vents, a cable notch, the carrier board's power
sockets and the microphone mount.

The panel fills the rear opening flush with the back edge and screws into
bosses on the top and bottom walls. It splits at REAR_SEAM_X, offset from the
shell seam so each panel half bridges the two shell halves and ties them.
Printed outer face down; the microphone frame grows up from the inner face.
The edition and maker are lettered into the outer face.
"""

from build123d import FontStyle, Part, Plane, Pos, Text, extrude

from waveform_one import carrier, layout
from waveform_one import parameters as p
from waveform_one.enclosure import REAR_FACE_DOWN
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box_between, rounded_prism, y_cylinder


def boss_sites() -> list[tuple[float, float]]:
    """(x, z) of every rear-panel screw."""
    (_, _, z_lo), (_, _, z_hi) = layout.interior_bounds()
    half = p.REAR_BOSS_PROTRUSION.v / 2
    return [(x, z_hi - half) for x in p.REAR_BOSS_X_TOP] + [(x, z_lo + half) for x in p.REAR_BOSS_X_BOTTOM]


def vent_regions() -> list[tuple[float, float]]:
    """(x_min, x_max) of each block of vertical slots, all centred on VENT_CENTRE_Z."""
    (x_lo, _, _), _ = layout.interior_bounds()
    seam = p.REAR_SEAM_X.v
    clear = p.VENT_PITCH.v
    mic_left = p.MIC_CENTRE_X.v - p.MIC_BOARD_W.v / 2 - p.MIC_POCKET_WALL.v
    return [
        (x_lo + p.CORNER_RADIUS.v, seam - clear),
        (seam + clear, mic_left - clear),
    ]


def vent_centre_z() -> float:
    """Upper part of the panel, where warm air collects."""
    (_, _, z_lo), (_, _, z_hi) = layout.interior_bounds()
    return z_hi - p.REAR_BOSS_PROTRUSION.v - p.VENT_PITCH.v - p.VENT_SLOT_LENGTH.v / 2


def back_plane(y: float, centre_z: float, centre_x: float) -> Plane:
    """A plane facing a viewer behind the case (+Y), so text on it reads left to right from there."""
    return Plane(origin=(centre_x, y, centre_z), x_dir=(-1, 0, 0), z_dir=(0, 1, 0))


def back_lettering() -> Part:
    """BACK_LINES, stacked and centred on BACK_TEXT_CENTRE, as cut into the outer face."""
    lines = [
        Text(line, size, font=p.BRAND_FONT, font_style=FontStyle.BOLD)
        for line, size in zip(p.BACK_LINES, p.BACK_FONT_SIZES)
    ]
    heights = [line.bounding_box().size.Y for line in lines]
    total = sum(heights) + p.BACK_LINE_GAP.v * (len(lines) - 1)
    top = total / 2
    sketch = None
    for line, height in zip(lines, heights):
        middle = line.bounding_box().center().Y
        moved = Pos(0, top - height / 2 - middle) * line
        sketch = moved if sketch is None else sketch + moved
        top -= height + p.BACK_LINE_GAP.v
    _, (_, y_inner, _) = layout.interior_bounds()
    plane = back_plane(y_inner + p.REAR_PANEL_THICKNESS.v, p.BACK_TEXT_CENTRE_Z.v, p.BACK_TEXT_CENTRE_X.v)
    return extrude(plane * sketch, amount=-p.BACK_TEXT_DEPTH.v)


def build_full_panel():
    (_, _, z_lo), (_, y_inner, z_hi) = layout.interior_bounds()
    clearance = p.REAR_PANEL_CLEARANCE.v
    width = layout.case_width() - 2 * (p.SHELL_WALL.v + clearance)
    height = z_hi - z_lo - 2 * clearance
    radius = p.CORNER_RADIUS.v - p.SHELL_WALL.v - clearance
    thickness = p.REAR_PANEL_THICKNESS.v
    panel = rounded_prism(width, height, radius, y_inner, thickness, (z_lo + z_hi) / 2)
    y0, y1 = y_inner - CUT_OVERSHOOT, y_inner + thickness + CUT_OVERSHOOT

    for x, z in boss_sites():
        panel -= y_cylinder(x, y0, y1, z, p.M3_CLEARANCE_HOLE.v)

    half_w, half_l = p.VENT_SLOT_WIDTH.v / 2, p.VENT_SLOT_LENGTH.v / 2
    zc = vent_centre_z()
    for x_min, x_max in vent_regions():
        x = x_min + half_w
        while x + half_w <= x_max:
            panel -= box_between((x - half_w, y0, zc - half_l), (x + half_w, y1, zc + half_l))
            x += p.VENT_PITCH.v

    port_half = p.CABLE_PORT_W.v / 2
    panel -= box_between(
        (p.CABLE_PORT_X.v - port_half, y0, z_lo - CUT_OVERSHOOT),
        (p.CABLE_PORT_X.v + port_half, y1, z_lo + p.CABLE_PORT_H.v),
    )

    # Openings for the carrier board's rear sockets (power input).
    for _, x_min, x_max, z_min, z_max in carrier.rear_openings():
        panel -= box_between((x_min, y0, z_min), (x_max, y1, z_max))

    panel -= back_lettering()

    mx, mz = p.MIC_CENTRE_X.v, p.MIC_CENTRE_Z.v
    panel -= y_cylinder(mx, y0, y1, mz, p.MIC_PORT_DIAMETER.v)
    inner_w = p.MIC_BOARD_W.v / 2 + p.POCKET_CLEARANCE.v
    inner_h = p.MIC_BOARD_H.v / 2 + p.POCKET_CLEARANCE.v
    wall = p.MIC_POCKET_WALL.v
    frame_y0 = y_inner - p.MIC_POCKET_HEIGHT.v
    frame = box_between((mx - inner_w - wall, frame_y0, mz - inner_h - wall), (mx + inner_w + wall, y_inner, mz + inner_h + wall))
    frame -= box_between((mx - inner_w, frame_y0 - CUT_OVERSHOOT, mz - inner_h), (mx + inner_w, y_inner, mz + inner_h))
    return panel + frame


def build_panels() -> list[PrintedPart]:
    panel = build_full_panel()
    half = layout.case_width() / 2
    o = CUT_OVERSHOOT
    y0, y1 = 0.0, p.CASE_DEPTH.v + o
    z0, z1 = -o, layout.case_height() + o
    seam = p.REAR_SEAM_X.v
    return [
        PrintedPart("rear_panel_left", panel & box_between((-half, y0, z0), (seam, y1, z1)), REAR_FACE_DOWN, "Outer face down."),
        PrintedPart("rear_panel_right", panel & box_between((seam, y0, z0), (half, y1, z1)), REAR_FACE_DOWN, "Outer face down."),
    ]
