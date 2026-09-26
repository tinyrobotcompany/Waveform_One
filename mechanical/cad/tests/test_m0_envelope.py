from dataclasses import replace

import pytest
from build123d import import_step

from waveform_one import parameters as p
from waveform_one.build import ELECTRONICS_NAME, ENVELOPE_NAME, REFERENCE_ONLY, ROOT_NAME, build_envelope, build_model
from waveform_one.export import export_model, layout_step_path
from waveform_one.validation import print_envelope_violations

TOL = 1e-6


def test_root_contains_envelope_and_electronics():
    model = build_model()
    assert model.label == ROOT_NAME
    assert [c.label for c in model.children] == [ENVELOPE_NAME, ELECTRONICS_NAME]
    assert {ENVELOPE_NAME, ELECTRONICS_NAME} <= REFERENCE_ONLY


def test_envelope_dimensions_follow_parameters():
    size = build_envelope().bounding_box().size
    assert size.X == pytest.approx(p.CASE_WIDTH.v, abs=TOL)
    assert size.Y == pytest.approx(p.CASE_DEPTH.v, abs=TOL)
    assert size.Z == pytest.approx(p.CASE_HEIGHT.v, abs=TOL)


def test_changing_case_width_changes_envelope(monkeypatch):
    monkeypatch.setattr(p, "CASE_WIDTH", replace(p.CASE_WIDTH, value=300.0))
    assert build_envelope().bounding_box().size.X == pytest.approx(300.0, abs=TOL)


def test_envelope_sits_on_the_datum():
    """Centred in X, front face on Y=0 extending rearward (+Y), base on Z=0."""
    box = build_envelope().bounding_box()
    assert box.min.X == pytest.approx(-box.max.X, abs=TOL)
    assert box.min.Y == pytest.approx(0.0, abs=TOL)
    assert box.max.Y == pytest.approx(p.CASE_DEPTH.v, abs=TOL)
    assert box.min.Z == pytest.approx(0.0, abs=TOL)


def test_envelope_is_too_wide_to_print_whole():
    """Documents why the enclosure must be modular."""
    violations = print_envelope_violations(build_envelope())
    assert [v[0] for v in violations] == ["X"]


def test_rebuilding_is_deterministic():
    first, second = build_model(), build_model()
    assert [c.label for c in first.children] == [c.label for c in second.children]
    assert first.bounding_box().size == second.bounding_box().size
    assert first.volume == pytest.approx(second.volume)


def test_export_round_trips_and_overwrites(tmp_path):
    model = build_model()
    export_model(model, tmp_path)
    export_model(model, tmp_path)
    step_files = list(tmp_path.rglob("*.step"))
    assert step_files == [layout_step_path(tmp_path)]
    assert len(list(tmp_path.rglob("*.png"))) == 1

    size = import_step(step_files[0]).bounding_box().size
    assert size.X == pytest.approx(p.CASE_WIDTH.v, abs=1e-3)
    assert size.Y == pytest.approx(p.CASE_DEPTH.v, abs=1e-3)
    assert size.Z == pytest.approx(p.CASE_HEIGHT.v, abs=1e-3)
