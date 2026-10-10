"""Removable tray carrying the Raspberry Pi, screwed onto four floor pads.

The tray stays on the flat part of the floor (inside the inner corner radius).
Pads sit near the tray corners, clear of the Pi's standoffs.
"""

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import raspberry_pi
from waveform_one.enclosure import AS_MODELLED
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box_between, z_cylinder


def tray_bounds() -> tuple[tuple[float, float], tuple[float, float]]:
    """((x_min, x_max), (y_min, y_max)) of the tray."""
    (x_min, _, _), _ = layout.interior_bounds()
    x, y, _ = layout.pi_origin()
    flat_floor_x = x_min + p.CORNER_RADIUS.v - p.SHELL_WALL.v
    margin = p.TRAY_MARGIN.v
    return (flat_floor_x, x + p.PI_BOARD_L.v + margin), (y - margin, y + p.PI_BOARD_W.v + margin)


def pad_centres() -> list[tuple[float, float]]:
    (x0, x1), (y0, y1) = tray_bounds()
    inset = p.TRAY_PAD_INSET.v
    return [(x, y) for x in (x0 + inset, x1 - inset) for y in (y0 + inset, y1 - inset)]


def build_tray() -> PrintedPart:
    (x0, x1), (y0, y1) = tray_bounds()
    top = layout.tray_top_z()
    bottom = top - p.TRAY_THICKNESS.v
    tray = box_between((x0, y0, bottom), (x1, y1, top))
    for x, y in pad_centres():
        tray -= z_cylinder(x, y, bottom - CUT_OVERSHOOT, top + CUT_OVERSHOOT, p.M3_CLEARANCE_HOLE.v)
    standoff_top = top + p.PI_STANDOFF_HEIGHT.v
    for x, y in raspberry_pi.mounting_hole_centres():
        tray += z_cylinder(x, y, top - CUT_OVERSHOOT, standoff_top, p.PI_STANDOFF_DIAMETER.v)
        tray -= z_cylinder(x, y, bottom - CUT_OVERSHOOT, standoff_top + CUT_OVERSHOOT, p.M25_SELF_TAP_HOLE.v)
    return PrintedPart("pi_tray", tray, AS_MODELLED, "Flat side down, standoffs up; no supports.")
