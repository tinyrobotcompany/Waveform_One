"""Entry point: validate parameters, build the Waveform One model and export it.

Run from mechanical/cad:
    uv run python -m waveform_one.build

The model is rebuilt from scratch on every run and the exports are overwritten,
so reruns can never accumulate duplicate geometry.
"""

import logging

from build123d import Align, Box, Color, Compound, Part, Pos, Rot

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import build_all
from waveform_one.enclosure import front_panel
from waveform_one.export import EXPORT_ROOT, export_model, export_printed_parts
from waveform_one.geometry import Placeholder, PrintedPart, box
from waveform_one.validation import require_printable, require_valid_layout, require_valid_parameters

log = logging.getLogger("waveform_one")

ROOT_NAME = "WaveformOne"
ENVELOPE_NAME = "Envelope"
ELECTRONICS_NAME = "Electronics"
FRONT_LAYOUT_NAME = "FrontLayoutMockup"
ALBUM_NAME = "AlbumReference"

# Components that exist for reference only and must never be exported for printing.
REFERENCE_ONLY = frozenset({ENVELOPE_NAME, ELECTRONICS_NAME, ALBUM_NAME})


def build_envelope() -> Box:
    """The product's outer reference volume, centred in X, front face on Y=0, base on Z=0."""
    envelope = Box(
        p.CASE_WIDTH.v,
        p.CASE_DEPTH.v,
        p.CASE_HEIGHT.v,
        align=(Align.CENTER, Align.MIN, Align.MIN),
    )
    envelope.label = ENVELOPE_NAME
    envelope.color = Color(0.55, 0.55, 0.60, 0.25)
    return envelope


def build_album_reference() -> Part:
    """A 12-inch sleeve standing in the rear slot, leaning back (+Y) by ALBUM_ANGLE_DEG."""
    size = p.ALBUM_WIDTH.v
    sleeve = box((-size / 2, 0, 0), (size, p.ALBUM_THICKNESS.v, size))
    y = p.CASE_DEPTH.v - p.ALBUM_REAR_OFFSET.v
    z = p.CASE_HEIGHT.v - p.ALBUM_SLOT_DEPTH.v
    sleeve = Pos(0, y, z) * Rot(-p.ALBUM_ANGLE_DEG.v, 0, 0) * sleeve
    sleeve.label = ALBUM_NAME
    sleeve.color = Color(0.85, 0.8, 0.7, 0.5)
    return sleeve


def build_printed_parts() -> list[PrintedPart]:
    return front_panel.build_all()


def build_model() -> tuple[Compound, list[PrintedPart]]:
    """The full reference assembly, plus the printable parts it contains."""
    require_valid_parameters()
    envelope = build_envelope()
    log.info("Built %s: %s", ENVELOPE_NAME, envelope.bounding_box())

    placeholders = build_all()
    printed = build_printed_parts()
    require_valid_layout(placeholders, layout.interior_bounds())
    in_enclosure = [Placeholder(pp.name, pp.body, pp.body.color) for pp in printed if not pp.mockup_only]
    require_valid_layout(placeholders + in_enclosure, None)
    require_printable(printed)
    for ph in placeholders:
        log.info("Placed %s: %s", ph.name, ph.body.bounding_box())
    for pp in printed:
        log.info("Printable %s on bed: %s", pp.name, pp.on_bed().bounding_box().size)

    electronics = Compound(label=ELECTRONICS_NAME, children=[ph.compound() for ph in placeholders])
    front = Compound(label=FRONT_LAYOUT_NAME, children=[pp.compound() for pp in printed])
    model = Compound(label=ROOT_NAME, children=[envelope, electronics, front, build_album_reference()])
    return model, printed


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    model, printed = build_model()
    for path in export_model(model, EXPORT_ROOT) + export_printed_parts(printed, EXPORT_ROOT):
        log.info("Wrote %s", path)


if __name__ == "__main__":
    main()
