"""Hole-size coupon: print it first, then set M3_INSERT_HOLE and M3_CLEARANCE_HOLE.

Front row: heat-set insert holes, smallest on the left (the chamfered corner).
Back row: M3 screw clearance holes, smallest on the left.
"""

from build123d import Axis, chamfer

from waveform_one import parameters as p
from waveform_one.enclosure import AS_MODELLED
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box, z_cylinder


def build_coupon() -> PrintedPart:
    pitch, thickness = p.COUPON_PITCH.v, p.COUPON_THICKNESS.v
    count = max(len(p.COUPON_INSERT_HOLES), len(p.COUPON_CLEARANCE_HOLES))
    length, depth = pitch * (count + 1), 2 * pitch + pitch
    coupon = box((0, 0, 0), (length, depth, thickness))
    for row, holes in ((1, p.COUPON_INSERT_HOLES), (2, p.COUPON_CLEARANCE_HOLES)):
        for i, diameter in enumerate(holes, start=1):
            coupon -= z_cylinder(i * pitch, row * pitch, -CUT_OVERSHOOT, thickness + CUT_OVERSHOOT, diameter)
    # Front-left vertical edge: the end with the smallest holes.
    marker = min(coupon.edges().filter_by(Axis.Z), key=lambda e: (e.center().X, e.center().Y))
    coupon = chamfer(marker, pitch / 2)
    return PrintedPart("calibration_coupon", coupon, AS_MODELLED, "Flat; no supports.")
