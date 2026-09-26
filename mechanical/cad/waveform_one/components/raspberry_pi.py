"""Raspberry Pi 4 Model B placeholder, mounted flat with space for a fan-heatsink.

As mounted, the USB/Ethernet end faces left (-X) and the USB-C/micro-HDMI edge
faces the rear panel (+Y).
"""

from build123d import Color, Cylinder, Pos

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "RaspberryPi"


def mounting_hole_centres() -> list[tuple[float, float]]:
    """(X, Y) of the four holes. Holes are measured from the non-USB (+X) end."""
    x, y, _ = layout.pi_origin()
    board_right = x + p.PI_PORT_OVERHANG.v + p.PI_BOARD_L.v
    inset = p.PI_HOLE_INSET.v
    xs = (board_right - inset, board_right - inset - p.PI_HOLE_PITCH_L.v)
    ys = (y + inset, y + inset + p.PI_HOLE_PITCH_W.v)
    return [(hx, hy) for hx in xs for hy in ys]


def build() -> Placeholder:
    x, y, z = layout.pi_origin()
    length, width, height = layout.pi_body_size()
    board_z = z + p.PI_UNDERSIDE_CLEARANCE.v
    board_x = x + p.PI_PORT_OVERHANG.v

    board = box((board_x, y, board_z), (p.PI_BOARD_L.v, width, p.PI_BOARD_T.v))
    for hx, hy in mounting_hole_centres():
        board -= Pos(hx, hy, board_z + p.PI_BOARD_T.v / 2) * Cylinder(
            p.PI_HOLE_DIAMETER.v / 2, p.PI_BOARD_T.v * 2
        )
    above_z = board_z + p.PI_BOARD_T.v
    body = board + box((x, y, above_z), (length, width, p.PI_HEIGHT_ABOVE_BOARD.v))

    (x_min, _, _), _ = layout.interior_bounds()
    usb = box((x_min, y, z), (x - x_min, width, height))
    rear_span = p.PI_REAR_PLUG_SPAN.v
    rear = box((x + length - rear_span, y + width, z), (rear_span, p.PI_REAR_PLUG_KEEPOUT.v, height))
    underside = box((board_x, y, z), (p.PI_BOARD_L.v, width, p.PI_UNDERSIDE_CLEARANCE.v))
    return Placeholder(
        NAME,
        body,
        Color(0.1, 0.5, 0.2),
        keepouts={"UsbToEsp32": usb, "UsbCPowerAndHdmi": rear, "Underside": underside},
    )
