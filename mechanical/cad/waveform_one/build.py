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
from waveform_one.calibration.coupon import build_coupon
from waveform_one.components import build_all
from waveform_one.enclosure import clips, pi_tray, rear_panel, record_holder, shell
from waveform_one.export import EXPORT_ROOT, export_model, export_printed_parts
from waveform_one.geometry import Placeholder, PrintedPart, box
from waveform_one.validation import require_printable, require_valid_layout, require_valid_parameters

log = logging.getLogger("waveform_one")

ROOT_NAME = "WaveformOne"
ENVELOPE_NAME = "Envelope"
ELECTRONICS_NAME = "Electronics"
ENCLOSURE_NAME = "Enclosure"
ALBUM_NAME = "AlbumReference"

# Components that exist for reference only and must never be exported for printing.
REFERENCE_ONLY = frozenset({ENVELOPE_NAME, ELECTRONICS_NAME, ALBUM_NAME})


def build_envelope() -> Box:
    """The case's outer bounding volume, centred in X, front face on Y=0, base on Z=0."""
    envelope = Box(*layout.case_size(), align=(Align.CENTER, Align.MIN, Align.MIN))
    envelope.label = ENVELOPE_NAME
    envelope.color = Color(0.55, 0.55, 0.60, 0.25)
    return envelope


def build_album_reference() -> Part:
    """A 12-inch sleeve standing in the holders' slots, leaning back by RECORD_ANGLE_DEG."""
    size = p.ALBUM_WIDTH.v
    sleeve = box((-size / 2, -p.ALBUM_THICKNESS.v / 2, 0), (size, p.ALBUM_THICKNESS.v, size))
    y, z = record_holder.slot_bottom()
    sleeve = Pos(0, y, z) * Rot(-p.RECORD_ANGLE_DEG.v, 0, 0) * sleeve
    sleeve.label = ALBUM_NAME
    sleeve.color = Color(0.85, 0.8, 0.7, 0.5)
    return sleeve


def build_enclosure_parts() -> list[PrintedPart]:
    """Every printed part of the enclosure, in assembly position."""
    return (
        shell.build_halves()
        + clips.build_clips()
        + [pi_tray.build_tray()]
        + rear_panel.build_panels()
        + record_holder.build_holders()
    )


def build_model() -> tuple[Compound, list[PrintedPart]]:
    """The full reference assembly, plus every part to print (including the coupon)."""
    require_valid_parameters()
    envelope = build_envelope()
    log.info("Case %s mm (W x D x H)", " x ".join(f"{v:.0f}" for v in layout.case_size()))

    placeholders = build_all()
    require_valid_layout(placeholders, layout.interior_bounds())
    enclosure = build_enclosure_parts()
    as_placeholders = [Placeholder(pp.name, pp.body, pp.body.color) for pp in enclosure]
    require_valid_layout(placeholders + as_placeholders, None)

    printed = enclosure + [build_coupon()]
    require_printable(printed)
    for pp in printed:
        log.info("Printable %s on bed: %s", pp.name, pp.on_bed().bounding_box().size)

    electronics = Compound(label=ELECTRONICS_NAME, children=[ph.compound() for ph in placeholders])
    enclosure_group = Compound(label=ENCLOSURE_NAME, children=[pp.compound() for pp in enclosure])
    model = Compound(label=ROOT_NAME, children=[envelope, electronics, enclosure_group, build_album_reference()])
    return model, printed


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    model, printed = build_model()
    for path in export_model(model, EXPORT_ROOT) + export_printed_parts(printed, EXPORT_ROOT):
        log.info("Wrote %s", path)


if __name__ == "__main__":
    main()
