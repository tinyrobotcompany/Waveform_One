"""Raspberry Pi 4 Model B placeholder, flat on its tray with space for a fan-heatsink.

As mounted, the USB/Ethernet end faces right (+X) and the USB-C/micro-HDMI edge
faces the rear panel (+Y).
"""

from build123d import Color, Cylinder, Pos

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "RaspberryPi"


def mounting_hole_centres() -> list[tuple[float, float]]:
    """(X, Y) of the four holes, measured from the non-USB (left) end."""
    x, y, _ = layout.pi_origin()
    inset = p.PI_HOLE_INSET.v
    xs = (x + inset, x + inset + p.PI_HOLE_PITCH_L.v)
    ys = (y + inset, y + inset + p.PI_HOLE_PITCH_W.v)
    return [(hx, hy) for hx in xs for hy in ys]


def build() -> Placeholder:
    x, y, z = layout.pi_origin()
    length, width, height = layout.pi_body_size()
    board = box((x, y, z), (p.PI_BOARD_L.v, width, p.PI_BOARD_T.v))
    for hx, hy in mounting_hole_centres():
        board -= Pos(hx, hy, z + p.PI_BOARD_T.v / 2) * Cylinder(p.PI_HOLE_DIAMETER.v / 2, 2 * p.PI_BOARD_T.v)
    body = board + box((x, y, z + p.PI_BOARD_T.v), (length, width, p.PI_HEIGHT_ABOVE_BOARD.v))

    usb = box((x + length, y, z), (p.PI_USB_PLUG_KEEPOUT.v, width, height))
    rear = box((x, y + width, z), (p.PI_REAR_PLUG_SPAN.v, p.PI_REAR_PLUG_KEEPOUT.v, height))
    return Placeholder(
        NAME,
        body,
        Color(0.1, 0.5, 0.2),
        keepouts={"UsbToEsp32": usb, "UsbCPowerAndHdmi": rear},
    )
