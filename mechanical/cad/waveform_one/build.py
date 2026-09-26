"""Entry point: validate parameters, build the Waveform One model and export it.

Run from mechanical/cad:
    uv run python -m waveform_one.build

The model is rebuilt from scratch on every run and the exports are overwritten,
so reruns can never accumulate duplicate geometry.
"""

import logging

from build123d import Align, Box, Color, Compound

from waveform_one import parameters as p
from waveform_one.export import EXPORT_ROOT, export_model
from waveform_one.validation import require_valid_parameters

log = logging.getLogger("waveform_one")

ROOT_NAME = "WaveformOne"
ENVELOPE_NAME = "Envelope"

# Components that exist for reference only and must never be exported for printing.
REFERENCE_ONLY = frozenset({ENVELOPE_NAME})


def build_envelope() -> Box:
    """The product's outer reference volume, centred in X, front face on Y=0, base on Z=0."""
    envelope = Box(
        p.CASE_WIDTH.value,
        p.CASE_DEPTH.value,
        p.CASE_HEIGHT.value,
        align=(Align.CENTER, Align.MIN, Align.MIN),
    )
    envelope.label = ENVELOPE_NAME
    envelope.color = Color(0.55, 0.55, 0.60, 0.25)
    return envelope


def build_model() -> Compound:
    require_valid_parameters()
    envelope = build_envelope()
    log.info("Built %s: %s", ENVELOPE_NAME, envelope.bounding_box())
    return Compound(label=ROOT_NAME, children=[envelope])


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    model = build_model()
    for path in export_model(model, EXPORT_ROOT):
        log.info("Wrote %s", path)


if __name__ == "__main__":
    main()
