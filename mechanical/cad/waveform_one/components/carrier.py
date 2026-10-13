"""Carrier board placeholder: the PCB, its parts and room for its cables.

The P4 cables leave the left edge; the HUB75 ribbon and microphone cable plug
in from above; the LED power wires leave the front terminal.
"""

from build123d import Color

from waveform_one import carrier
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box_between

NAME = "CarrierBoard"


def build() -> Placeholder:
    (x0, x1), (y0, y1) = carrier.bounds()
    body = box_between((x0, y0, carrier.bottom_z()), (x1, y1, carrier.top_z()))
    for item in carrier.parts():
        low, high = carrier.part_box(item)
        if item["ref"] == "U1":
            # The module's courtyard includes the antenna keep-out beyond the
            # board edge; only the module itself is solid.
            high = (min(high[0], x1), high[1], high[2])
        body += box_between(low, high)

    keepouts = {}
    side = [carrier.part_box(carrier.part(ref)) for ref in ("J3", "J4")]
    tallest = max(high[2] for _, high in side)
    keepouts["CablesToP4"] = box_between(
        (x0 - p.CARRIER_SIDE_CABLE_KEEPOUT.v, min(low[1] for low, _ in side), carrier.bottom_z()),
        (x0, max(high[1] for _, high in side), tallest + p.CARRIER_OPENING_CLEARANCE.v),
    )
    for ref, name in (("J5", "Hub75Ribbon"), ("J7", "MicrophoneCable")):
        (lx, ly, _), (hx, hy, hz) = carrier.part_box(carrier.part(ref))
        keepouts[name] = box_between((lx, ly, hz), (hx, hy, hz + p.CARRIER_PLUG_KEEPOUT.v))
    (lx, ly, lz), (hx, _, hz) = carrier.part_box(carrier.part("J6"))
    keepouts["LedPowerWires"] = box_between((lx, ly - p.CARRIER_WIRE_KEEPOUT.v, lz), (hx, ly, hz))
    return Placeholder(NAME, body, Color(0.1, 0.35, 0.2), keepouts=keepouts)
