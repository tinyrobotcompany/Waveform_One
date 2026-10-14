"""The cabinet: rounded walls and the black front plate as one printed shell.

The shell is a rounded tube open at the rear, closed at the front by the front
plate with the screen and LED windows. It splits into two halves at the middle
of the divider between the windows, where the concept already shows a line.
A tongue on the left half (front plate, top and bottom walls) slots into a
groove in the right half for alignment; the halves are glued, and the rear
panel screws across the joint.

Both halves print front face down: the front gets the bed's finish, every wall
grows straight up, and wall-mounted bosses have 45-degree undersides.

Front-face details (window chamfers, the name) are cut into the
face that sits on the bed.
"""

from build123d import Axis, Part, Text, FontStyle, chamfer, extrude

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import lcd
from waveform_one import carrier
from waveform_one.enclosure import FRONT_FACE_DOWN, clips, record_holder, rear_panel
from waveform_one.geometry import (
    CUT_OVERSHOOT,
    Band,
    PrintedPart,
    box_between,
    front_plane,
    rounded_prism,
    sloped_pad,
    y_cylinder,
    z_cylinder,
)


def screen_window() -> tuple[tuple[float, float], tuple[float, float]]:
    (x0, x1), (z0, z1) = lcd.active_area()
    m = p.SCREEN_WINDOW_MARGIN.v
    return (x0 - m, x1 + m), (z0 - m, z1 + m)


def led_window() -> tuple[tuple[float, float], tuple[float, float]]:
    x, _, z = layout.led_origin()
    lip = p.LED_WINDOW_LIP.v
    return (x + lip, x + p.LED_W.v - lip), (z + lip, z + p.LED_H.v - lip)


def brand_centre() -> tuple[float, float]:
    """(x, z) of the name: centred on the strip of front below the LED window.

    That strip runs from the case's bottom edge to the window's chamfered edge.
    """
    (x0, x1), (z_window, _) = led_window()
    return (x0 + x1) / 2, (z_window - p.WINDOW_CHAMFER.v) / 2


def _outer_shell() -> Part:
    width, depth, height = layout.case_size()
    t, r = p.SHELL_WALL.v, p.CORNER_RADIUS.v
    outer = rounded_prism(width, height, r, 0, depth, height / 2)
    cavity = rounded_prism(width - 2 * t, height - 2 * t, r - t, p.FRONT_THICKNESS.v, depth, height / 2)
    shell = outer - cavity
    front = shell.faces().sort_by(Axis.Y)[0]
    return chamfer(front.outer_wire().edges(), p.FRONT_EDGE_CHAMFER.v)


def _cut_windows(shell: Part) -> Part:
    y0, y1 = -CUT_OVERSHOOT, p.FRONT_THICKNESS.v + CUT_OVERSHOOT
    for (x0, x1), (z0, z1) in (screen_window(), led_window()):
        shell -= box_between((x0, y0, z0), (x1, y1, z1))
    front = shell.faces().sort_by(Axis.Y)[0]
    return chamfer(front.inner_wires().edges(), p.WINDOW_CHAMFER.v)


def brand_name() -> Part:
    """The name, as cut into the front face; the cut is filled with the white inlay."""
    x, z = brand_centre()
    text = Text(p.BRAND_TEXT, p.BRAND_FONT_SIZE.v, font=p.BRAND_FONT, font_style=FontStyle.BOLD)
    return extrude(front_plane(0, z, x) * text, amount=-p.BRAND_DEPTH.v)


def _pockets() -> Part:
    """Locating walls around each component on the back of the front plate."""
    c, wall = p.POCKET_CLEARANCE.v, p.POCKET_WALL.v
    y0 = p.FRONT_THICKNESS.v - CUT_OVERSHOOT
    y1 = p.FRONT_THICKNESS.v + p.POCKET_HEIGHT.v
    walls = None
    for (x, _, z), w, h in (
        (layout.screen_origin(), p.SCREEN_W.v, p.SCREEN_H.v),
        (layout.diffuser_origin(), p.LED_W.v, p.LED_H.v),
    ):
        outer = box_between((x - c - wall, y0, z - c - wall), (x + w + c + wall, y1, z + h + c + wall))
        inner = box_between((x - c, y0 - CUT_OVERSHOOT, z - c), (x + w + c, y1 + CUT_OVERSHOOT, z + h + c))
        ring = outer - inner
        walls = ring if walls is None else walls + ring
    return walls


def _rear_bosses():
    """Bosses on the top and bottom walls for the rear panel, with insert holes."""
    (_, _, z_lo), (_, y_inner, z_hi) = layout.interior_bounds()
    prot, flat, w = p.REAR_BOSS_PROTRUSION.v, p.REAR_BOSS_FLAT.v, p.REAR_BOSS_WIDTH.v
    bosses, holes = [], []
    for xs, wall_z, up in ((p.REAR_BOSS_X_TOP, z_hi, False), (p.REAR_BOSS_X_BOTTOM, z_lo, True)):
        for x in xs:
            bosses.append(sloped_pad(x, w, y_inner - flat, y_inner, wall_z, prot, up))
    for x, z in rear_panel.boss_sites():
        holes.append(y_cylinder(x, y_inner - p.M3_INSERT_DEPTH.v, y_inner + CUT_OVERSHOOT, z, p.M3_INSERT_HOLE.v))
    return bosses, holes


def _carrier_posts():
    """Floor posts with heat-set inserts under the carrier board's four holes."""
    _, _, z_floor = layout.interior_bounds()[0]
    size, height = p.CARRIER_POST_SIZE.v, carrier.board()["standoff_height"]
    posts, holes = [], []
    for x, y in carrier.hole_centres():
        posts.append(sloped_pad(x, size, y - size / 2, y + size / 2, z_floor, height, up=True))
        holes.append(z_cylinder(x, y, z_floor + height - p.M3_INSERT_DEPTH.v, z_floor + height + CUT_OVERSHOOT, p.M3_INSERT_HOLE.v))
    return posts, holes


def floor_vent_region() -> tuple[tuple[float, float], tuple[float, float]]:
    """((x_min, x_max), (y_min, y_max)): under the P4 board, left of the shell seam."""
    (x_min, _, _), _ = layout.interior_bounds()
    clear = p.VENT_PITCH.v
    flat_floor_x = x_min + p.CORNER_RADIUS.v - p.SHELL_WALL.v
    return (flat_floor_x + clear, layout.seam_x() - clear), (p.FLOOR_VENT_Y_MIN.v, p.FLOOR_VENT_Y_MAX.v)


def _floor_vents() -> list[Part]:
    (x0, x1), (y0, y1) = floor_vent_region()
    half_w = p.VENT_SLOT_WIDTH.v / 2
    half_l = min(p.VENT_SLOT_LENGTH.v, y1 - y0) / 2
    yc = (y0 + y1) / 2
    vents, x = [], x0 + half_w
    while x + half_w <= x1:
        # Slots run front-to-back (along the print's vertical axis) so their tops bridge only 2.5 mm.
        vents.append(box_between((x - half_w, yc - half_l, -CUT_OVERSHOOT), (x + half_w, yc + half_l, p.SHELL_WALL.v + CUT_OVERSHOOT)))
        x += p.VENT_PITCH.v
    return vents


def _top_and_bottom_holes() -> list[Part]:
    height = layout.case_height()
    o = CUT_OVERSHOOT
    inside = height - p.SHELL_WALL.v
    # Blind pockets from inside for the holder magnets: the top surface stays unbroken.
    holes = [
        z_cylinder(x, y, inside - o, inside + record_holder.magnet_pocket_depth(), record_holder.magnet_pocket_diameter())
        for x, y in record_holder.magnet_positions()
    ]
    inset, r, depth = p.FOOT_INSET.v, p.FOOT_RECESS_DIAMETER.v, p.FOOT_RECESS_DEPTH.v
    half = layout.case_width() / 2
    for x in (-half + inset, half - inset):
        for y in (inset, p.CASE_DEPTH.v - inset):
            holes.append(z_cylinder(x, y, -o, depth, r))
    return holes


def build_full_shell() -> Part:
    shell = _cut_windows(_outer_shell())
    shell -= brand_name()
    shell += _pockets()
    for features, cuts in (clips.build_bosses(), _rear_bosses(), _carrier_posts()):
        for feature in features:
            shell += feature
        for cut in cuts:
            shell -= cut
    for cut in _floor_vents() + _top_and_bottom_holes():
        shell -= cut
    return shell


def tongue_boxes(grow: float = 0.0) -> list[Part]:
    """Tongue from the left half into the right, grown by `grow` per side for the groove."""
    width, depth, height = layout.case_size()
    seam = layout.seam_x()
    t, front = p.SHELL_WALL.v, p.FRONT_THICKNESS.v
    inset, reach = p.TONGUE_END_INSET.v, p.TONGUE_DEPTH.v + 2 * grow
    tt = p.TONGUE_THICKNESS.v / 2 + grow  # half-thickness, centred in the plate or wall
    (_, _, z_lo), (_, _, z_hi) = layout.interior_bounds()
    x0, x1 = seam - CUT_OVERSHOOT, seam + reach
    return [
        box_between((x0, front / 2 - tt, z_lo + inset - grow), (x1, front / 2 + tt, z_hi - inset + grow)),
        box_between((x0, inset - grow, height - t / 2 - tt), (x1, depth - inset + grow, height - t / 2 + tt)),
        box_between((x0, inset - grow, t / 2 - tt), (x1, depth - inset + grow, t / 2 + tt)),
    ]


def stripe_slabs() -> list[tuple[float, float]]:
    """Front-to-back extent (Y) of each colour stripe."""
    width = p.STRIPE_WIDTH.v
    return [(start, start + width) for start in (p.STRIPE_1_FROM_FRONT.v, p.STRIPE_2_FROM_FRONT.v)]


def colour_bands(name: str, body: Part, inlay: Part | None = None) -> tuple[Band, ...]:
    """The shell half as its black body plus one band per colour stripe, and the name inlay.

    Each stripe is the whole cross-section between two depths: printed front
    face down, every layer is one colour, so the printer only changes filament
    at the stripe edges. The name inlay fills the lettering cut into the front
    face; it sits in the first few layers, the only ones that mix two colours.
    """
    width, depth, height = layout.case_size()
    o = CUT_OVERSHOOT
    base = body
    stripes = []
    for index, (y0, y1) in enumerate(stripe_slabs(), start=1):
        slab = box_between((-width / 2 - o, y0, -o), (width / 2 + o, y1, height + o))
        stripes.append(Band(f"{name}_stripe_{index}", index + 1, body & slab))
        base = base - slab
    if inlay is None:
        return (Band(f"{name}_body", 1, base), *stripes)
    return (Band(f"{name}_body", 1, base - inlay), *stripes, Band(f"{name}_name", p.NAME_FILAMENT, inlay))


def build_halves() -> list[PrintedPart]:
    shell = build_full_shell()
    width, depth, height = layout.case_size()
    o = CUT_OVERSHOOT
    seam = layout.seam_x()
    left = shell & box_between((-width / 2 - o, -o, -o), (seam, depth + o, height + o))
    right = shell & box_between((seam, -o, -o), (width / 2 + o, depth + o, height + o))
    for tongue in tongue_boxes():
        left += tongue
    for groove in tongue_boxes(grow=p.JOINT_CLEARANCE.v):
        right -= groove
    note = "Front face down on the bed; no supports."
    halves = []
    name = brand_name()
    for label, body, (x0, x1) in (
        ("shell_left", left, (-width / 2 - o, seam)),
        ("shell_right", right, (seam, width / 2 + o)),
    ):
        inlay = name & box_between((x0, -o, -o), (x1, depth + o, height + o))
        if inlay.volume > 0:  # the name lies in this half: print it filled, in white
            body += inlay
            bands = colour_bands(label, body, inlay)
        else:
            bands = colour_bands(label, body)
        halves.append(PrintedPart(label, body, FRONT_FACE_DOWN, note, bands=bands))
    return halves
