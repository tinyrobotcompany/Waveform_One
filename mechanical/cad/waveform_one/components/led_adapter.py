"""LED adapter board placeholder: the PCB, its parts and room for its cables.

The two cables from the P4's GPIO sockets plug into J1 and J2 from above; the
panel's ribbon plugs into J3 from above; the 5 V wires enter J4 from the front.
"""

from build123d import Color

from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box_between
from waveform_one.pcb import LED_ADAPTER as board

NAME = "LedAdapter"


def build() -> Placeholder:
    (x0, x1), (y0, y1) = board.bounds()
    body = box_between((x0, y0, board.bottom_z()), (x1, y1, board.top_z()))
    for item in board.parts():
        low, high = board.part_box(item)
        body += box_between(low, high)

    keepouts = {}
    for ref, name in (("J1", "CableToP4SocketP3"), ("J2", "CableToP4SocketP1"), ("J3", "Hub75Ribbon")):
        (lx, ly, _), (hx, hy, hz) = board.part_box(board.part(ref))
        keepouts[name] = box_between((lx, ly, hz), (hx, hy, hz + p.CARRIER_PLUG_KEEPOUT.v))
    (lx, ly, lz), (hx, _, hz) = board.part_box(board.part("J4"))
    keepouts["PowerWires"] = box_between((lx, ly - p.ADAPTER_WIRE_KEEPOUT.v, lz), (hx, ly, hz))
    return Placeholder(NAME, body, Color(0.1, 0.3, 0.25), keepouts=keepouts)
