"""INMP441 breakout on the rear panel's inner face, sound port facing out (+Y)."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "Microphone"


def build() -> Placeholder:
    x, y, z = layout.mic_origin()
    w, t, h = p.MIC_BOARD_W.v, p.MIC_BOARD_T.v, p.MIC_BOARD_H.v
    wires = box((x, y - p.MIC_WIRE_KEEPOUT.v, z), (w, p.MIC_WIRE_KEEPOUT.v, h))
    return Placeholder(
        NAME,
        box((x, y, z), (w, t, h)),
        Color(0.45, 0.2, 0.6),
        keepouts={"HeaderAndI2sWires": wires},
    )
