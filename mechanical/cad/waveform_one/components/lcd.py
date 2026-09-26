"""Official Raspberry Pi 7-inch Touchscreen (original, DSI) placeholder."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "Display"


def build() -> Placeholder:
    x, y, z = layout.display_origin()
    w, d, h = p.DISPLAY_W.v, p.DISPLAY_D.v, p.DISPLAY_H.v
    body = box((x, y, z), (w, d, h))

    active_x = x + (w - p.DISPLAY_ACTIVE_W.v) / 2 + p.DISPLAY_ACTIVE_OFFSET_X.v
    active_z = z + (h - p.DISPLAY_ACTIVE_H.v) / 2 + p.DISPLAY_ACTIVE_OFFSET_Z.v
    # Thin marker just in front of the glass, inside the recess behind the bezel.
    marker_t = min(0.2, p.DISPLAY_RECESS.v)
    active = box((active_x, y - marker_t, active_z), (p.DISPLAY_ACTIVE_W.v, marker_t, p.DISPLAY_ACTIVE_H.v))

    cable_w, cable_h = p.DISPLAY_CABLE_KEEPOUT_W.v, p.DISPLAY_CABLE_KEEPOUT_H.v
    cable = box(
        (-cable_w / 2, y + d, z + (h - cable_h) / 2),
        (cable_w, p.DISPLAY_CABLE_KEEPOUT_D.v, cable_h),
    )
    return Placeholder(
        NAME,
        body,
        Color(0.25, 0.28, 0.35),
        keepouts={"DSIRibbon": cable},
        markers={"ActiveArea": active},
    )
