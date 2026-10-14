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
from waveform_one.enclosure import clips, rear_panel, record_holder, shell
from waveform_one.export import EXPORT_ROOT, export_model, export_print_job, export_printed_parts
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


def _clip_names(component: str) -> tuple[str, ...]:
    return tuple(f"clip_{component}_{side}_{edge}" for side in ("left", "right") for edge in ("top", "bottom"))


# Print jobs, in the order to print them. Each becomes one ready-to-open 3MF.
# The coupon comes first: its results set the hole sizes in everything else.
PRINT_JOBS = (
    ("1_calibration_coupon", ("calibration_coupon",)),
    ("2_shell_left", ("shell_left",)),
    ("3_shell_right", ("shell_right",)),
    ("4_small_parts_a", ("rear_panel_left",) + _clip_names("screen")),
    ("5_small_parts_b", ("rear_panel_right", "record_holder_left", "record_holder_right") + _clip_names("led")),
)


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
    paths = export_model(model, EXPORT_ROOT) + export_printed_parts(printed, EXPORT_ROOT)
    by_name = {pp.name: pp for pp in printed}
    for job, names in print_jobs(by_name):
        paths.append(export_print_job(job, names, EXPORT_ROOT))
    for path in paths:
        log.info("Wrote %s", path)


def print_jobs(by_name: dict[str, PrintedPart]) -> list[tuple[str, list[PrintedPart]]]:
    """PRINT_JOBS resolved to parts; every printed part must appear in exactly one job."""
    listed = [name for _, names in PRINT_JOBS for name in names]
    missing, extra = set(by_name) - set(listed), set(listed) - set(by_name)
    duplicated = {name for name in listed if listed.count(name) > 1}
    if missing or extra or duplicated:
        raise RuntimeError(f"PRINT_JOBS mismatch: missing {missing}, unknown {extra}, repeated {duplicated}")
    return [(job, [by_name[name] for name in names]) for job, names in PRINT_JOBS]


if __name__ == "__main__":
    main()
