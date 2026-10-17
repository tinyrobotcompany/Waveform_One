"""Two holders on top of the case that stand a 12-inch sleeve upright.

Each holder has a slot leaning back RECORD_ANGLE_DEG. The holders are
optional: they attach with magnets, so the top stays plain without them. Two
disc magnets sit in pockets in each holder's base, over two more in blind
pockets under the shell's top wall.
"""

from build123d import Axis, Pos, Rot, fillet

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.enclosure import AS_MODELLED
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box, z_cylinder


def holder_front_y() -> float:
    return p.CASE_DEPTH.v - p.HOLDER_REAR_INSET.v - p.HOLDER_DEPTH.v


def slot_bottom() -> tuple[float, float]:
    """(y, z) of the slot floor's centre line, where a sleeve's lower edge rests."""
    y = holder_front_y() + p.HOLDER_DEPTH.v / 2
    return y, layout.case_height() + p.HOLDER_HEIGHT.v - p.RECORD_SLOT_DEPTH.v


def magnet_positions() -> list[tuple[float, float]]:
    """(x, y) of every holder magnet; the shell's top wall has a matching pocket under each."""
    y = holder_front_y() + p.HOLDER_MAGNET_INSET.v
    half = p.HOLDER_MAGNET_SPACING.v / 2
    return [(sign * p.HOLDER_X.v + dx, y) for sign in (-1, 1) for dx in (-half, half)]


def magnet_pocket_diameter() -> float:
    return p.MAGNET_DIAMETER.v + p.MAGNET_HOLE_CLEARANCE.v


def magnet_pocket_depth() -> float:
    return p.MAGNET_THICKNESS.v + p.MAGNET_POCKET_EXTRA.v


def build_holders() -> list[PrintedPart]:
    top = layout.case_height()
    slot_y, slot_z = slot_bottom()
    slot_height = p.RECORD_SLOT_DEPTH.v + p.HOLDER_HEIGHT.v  # generously past the top
    parts = []
    for side, sign in (("left", -1), ("right", 1)):
        cx = sign * p.HOLDER_X.v
        body = box((cx - p.HOLDER_LENGTH.v / 2, holder_front_y(), top), (p.HOLDER_LENGTH.v, p.HOLDER_DEPTH.v, p.HOLDER_HEIGHT.v))
        body = fillet(body.edges().filter_by(Axis.Z), p.HOLDER_CORNER_RADIUS.v)
        cutter = box(
            (-p.HOLDER_LENGTH.v / 2 - CUT_OVERSHOOT, -p.RECORD_SLOT_WIDTH.v / 2, 0),
            (p.HOLDER_LENGTH.v + 2 * CUT_OVERSHOOT, p.RECORD_SLOT_WIDTH.v, slot_height),
        )
        body -= Pos(cx, slot_y, slot_z) * Rot(-p.RECORD_ANGLE_DEG.v, 0, 0) * cutter
        for x, y in magnet_positions():
            if abs(x - cx) < p.HOLDER_LENGTH.v / 2:  # this holder's own magnets, flush with its base
                body -= z_cylinder(x, y, top - CUT_OVERSHOOT, top + magnet_pocket_depth(), magnet_pocket_diameter())
        parts.append(PrintedPart(f"record_holder_{side}", body, AS_MODELLED, "Base down, slot up; no supports."))
    return parts
