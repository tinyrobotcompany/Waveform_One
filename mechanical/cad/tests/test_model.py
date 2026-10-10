import pytest
from build123d import import_stl, import_step

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.build import (
    ALBUM_NAME,
    ELECTRONICS_NAME,
    ENCLOSURE_NAME,
    ENVELOPE_NAME,
    REFERENCE_ONLY,
    ROOT_NAME,
    build_envelope,
    build_model,
)
from waveform_one.export import export_model, layout_step_path

TOL = 1e-6


@pytest.fixture(scope="module")
def built():
    return build_model()


def test_root_contains_the_expected_groups(built):
    model, _ = built
    assert model.label == ROOT_NAME
    assert [c.label for c in model.children] == [ENVELOPE_NAME, ELECTRONICS_NAME, ENCLOSURE_NAME, ALBUM_NAME]
    assert ENCLOSURE_NAME not in REFERENCE_ONLY


def test_envelope_matches_the_case_and_sits_on_the_datum():
    box = build_envelope().bounding_box()
    assert tuple(box.size) == pytest.approx(layout.case_size())
    assert box.min.X == pytest.approx(-box.max.X, abs=TOL)
    assert (box.min.Y, box.min.Z) == pytest.approx((0.0, 0.0), abs=TOL)


def test_printed_parts_include_the_coupon(built):
    _, printed = built
    assert printed[-1].name == "calibration_coupon"
    assert len({pp.name for pp in printed}) == len(printed)


def test_export_round_trips_and_overwrites(tmp_path, built):
    model, _ = built
    export_model(model, tmp_path)
    export_model(model, tmp_path)
    step_files = list(tmp_path.rglob("*.step"))
    assert step_files == [layout_step_path(tmp_path)]
    assert len(list(tmp_path.rglob("*.png"))) == 1
    reimported = import_step(step_files[0])
    assert len(reimported.solids()) == len(model.solids())
    original, loaded = model.bounding_box(), reimported.bounding_box()
    assert tuple(loaded.min) == pytest.approx(tuple(original.min), abs=1e-3)
    assert tuple(loaded.max) == pytest.approx(tuple(original.max), abs=1e-3)
