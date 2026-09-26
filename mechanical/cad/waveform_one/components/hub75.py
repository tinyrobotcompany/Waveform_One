"""Waveshare RGB-Matrix-P3-64x32 HUB75 panel and the owner's diffuser."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

MATRIX_NAME = "Matrix"
DIFFUSER_NAME = "Diffuser"


def build_matrix() -> Placeholder:
    x, y, z = layout.matrix_origin()
    w, d, h = p.MATRIX_W.v, p.MATRIX_D.v, p.MATRIX_H.v
    rear = box((x, y + d, z), (w, p.MATRIX_REAR_KEEPOUT.v, h))
    return Placeholder(
        MATRIX_NAME,
        box((x, y, z), (w, d, h)),
        Color(0.08, 0.08, 0.08),
        keepouts={"ConnectorsAndPower": rear},
    )


def build_diffuser() -> Placeholder:
    origin = layout.diffuser_origin()
    size = (p.MATRIX_W.v, p.DIFFUSER_THICKNESS.v, p.MATRIX_H.v)
    return Placeholder(DIFFUSER_NAME, box(origin, size), Color(0.9, 0.9, 0.95))
