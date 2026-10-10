"""Waveshare RGB-Matrix-P3-64x32 HUB75 panel and the owner's diffuser in front of it."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

LED_NAME = "LedPanel"
DIFFUSER_NAME = "Diffuser"


def build_led() -> Placeholder:
    x, y, z = layout.led_origin()
    w, d, h = p.LED_W.v, p.LED_D.v, p.LED_H.v
    inset = p.KEEPOUT_EDGE_INSET.v
    rear = box((x + inset, y + d, z + inset), (w - 2 * inset, p.LED_REAR_KEEPOUT.v, h - 2 * inset))
    return Placeholder(
        LED_NAME,
        box((x, y, z), (w, d, h)),
        Color(0.08, 0.08, 0.08),
        keepouts={"ConnectorsAndPower": rear},
    )


def build_diffuser() -> Placeholder:
    size = (p.LED_W.v, p.DIFFUSER_THICKNESS.v, p.LED_H.v)
    return Placeholder(DIFFUSER_NAME, box(layout.diffuser_origin(), size), Color(0.9, 0.9, 0.95))
