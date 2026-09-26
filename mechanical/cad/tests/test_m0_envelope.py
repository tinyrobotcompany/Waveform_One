import pytest
from build123d import import_step

from waveform_one import parameters as p
from waveform_one.build import ENVELOPE_NAME, REFERENCE_ONLY, ROOT_NAME, build_model
from waveform_one.export import export_model, reference_step_path
from waveform_one.validation import print_envelope_violations

TOL = 1e-6


def envelope(model):
    (child,) = model.children
    return child


def test_root_contains_only_the_envelope():
    model = build_model()
    assert model.label == ROOT_NAME
    assert [c.label for c in model.children] == [ENVELOPE_NAME]
    assert ENVELOPE_NAME in REFERENCE_ONLY


def test_envelope_dimensions_follow_parameters():
    size = envelope(build_model()).bounding_box().size
    assert size.X == pytest.approx(p.CASE_WIDTH.value, abs=TOL)
    assert size.Y == pytest.approx(p.CASE_DEPTH.value, abs=TOL)
    assert size.Z == pytest.approx(p.CASE_HEIGHT.value, abs=TOL)


def test_envelope_sits_on_the_datum():
    """Centred in X, front face on Y=0 extending rearward (+Y), base on Z=0."""
    box = envelope(build_model()).bounding_box()
    assert box.min.X == pytest.approx(-box.max.X, abs=TOL)
    assert box.min.Y == pytest.approx(0.0, abs=TOL)
    assert box.max.Y == pytest.approx(p.CASE_DEPTH.value, abs=TOL)
    assert box.min.Z == pytest.approx(0.0, abs=TOL)


def test_envelope_is_one_solid_with_expected_volume():
    solids = envelope(build_model()).solids()
    assert len(solids) == 1
    expected = p.CASE_WIDTH.value * p.CASE_DEPTH.value * p.CASE_HEIGHT.value
    assert solids[0].volume == pytest.approx(expected, rel=1e-9)


def test_rebuilding_is_deterministic():
    first, second = build_model(), build_model()
    assert len(second.children) == 1
    assert envelope(first).bounding_box().size == envelope(second).bounding_box().size


def test_envelope_is_too_big_to_print_whole():
    """Documents why the enclosure must be modular: width and height exceed 245 mm."""
    violations = print_envelope_violations(envelope(build_model()))
    assert any(v.startswith("X") for v in violations)
    assert any(v.startswith("Z") for v in violations)


def test_export_round_trips_and_overwrites(tmp_path):
    model = build_model()
    export_model(model, tmp_path)
    export_model(model, tmp_path)
    step_files = list(tmp_path.rglob("*.step"))
    assert step_files == [reference_step_path(tmp_path)]

    size = import_step(step_files[0]).bounding_box().size
    assert size.X == pytest.approx(p.CASE_WIDTH.value, abs=1e-3)
    assert size.Y == pytest.approx(p.CASE_DEPTH.value, abs=1e-3)
    assert size.Z == pytest.approx(p.CASE_HEIGHT.value, abs=1e-3)
