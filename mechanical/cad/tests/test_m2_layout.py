from dataclasses import replace

import pytest

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import build_all, raspberry_pi
from waveform_one.validation import ValidationError, collision_errors, outside_bounds_errors, require_valid_layout

EXPECTED = ["Diffuser", "Matrix", "Display", "RaspberryPi", "ESP32", "Microphone"]


@pytest.fixture(scope="module")
def parts():
    return {ph.name: ph for ph in build_all()}


def box(parts, name):
    return parts[name].body.bounding_box()


def test_each_required_component_exists_once(parts):
    assert list(parts) == EXPECTED


def test_encoders_are_deferred(parts):
    assert p.ENCODERS_ENABLED.v == 0
    assert not any("ncoder" in name for name in parts)


def test_everything_fits_inside_the_walls(parts):
    assert outside_bounds_errors(list(parts.values()), layout.interior_bounds()) == []


def test_no_component_collides_or_blocks_a_keepout(parts):
    assert collision_errors(list(parts.values())) == []


def test_display_sits_above_matrix_both_centred(parts):
    display, matrix = box(parts, "Display"), box(parts, "Matrix")
    assert display.min.Z == pytest.approx(matrix.max.Z + p.DISPLAY_MATRIX_GAP.v)
    for b in (display, matrix):
        assert b.center().X == pytest.approx(0.0, abs=1e-6)


def test_display_top_leaves_the_minimum_margin(parts):
    assert p.CASE_HEIGHT.v - box(parts, "Display").max.Z >= p.MIN_TOP_MARGIN.v


def test_front_components_start_behind_the_bezel_in_order(parts):
    diffuser, matrix, display = box(parts, "Diffuser"), box(parts, "Matrix"), box(parts, "Display")
    assert diffuser.min.Y == pytest.approx(p.FRONT_BEZEL_THICKNESS.v)
    assert matrix.min.Y == pytest.approx(diffuser.max.Y + p.DIFFUSER_TO_MATRIX_GAP.v)
    assert display.min.Y == pytest.approx(p.FRONT_BEZEL_THICKNESS.v + p.DISPLAY_RECESS.v)


def test_matrix_and_display_use_parameter_sizes(parts):
    matrix, display = box(parts, "Matrix").size, box(parts, "Display").size
    assert (matrix.X, matrix.Y, matrix.Z) == pytest.approx((p.MATRIX_W.v, p.MATRIX_D.v, p.MATRIX_H.v))
    assert (display.X, display.Y, display.Z) == pytest.approx((p.DISPLAY_W.v, p.DISPLAY_D.v, p.DISPLAY_H.v))


def test_pi_has_four_mounting_holes_on_the_datasheet_pattern(parts):
    holes = raspberry_pi.mounting_hole_centres()
    xs, ys = sorted({x for x, _ in holes}), sorted({y for _, y in holes})
    assert len(holes) == 4
    assert xs[1] - xs[0] == pytest.approx(p.PI_HOLE_PITCH_L.v)
    assert ys[1] - ys[0] == pytest.approx(p.PI_HOLE_PITCH_W.v)


def test_microphone_faces_the_rear_panel(parts):
    _, hi = layout.interior_bounds()
    assert box(parts, "Microphone").max.Y == pytest.approx(hi[1])


def test_moving_the_pi_into_the_display_is_caught(monkeypatch):
    monkeypatch.setattr(p, "PI_REAR_PLUG_KEEPOUT", replace(p.PI_REAR_PLUG_KEEPOUT, value=100.0))
    with pytest.raises(ValidationError, match="RaspberryPi body overlaps Display"):
        require_valid_layout(build_all(), layout.interior_bounds())


def test_component_pushed_through_a_wall_is_caught(monkeypatch):
    monkeypatch.setattr(p, "MIC_CENTRE_X", replace(p.MIC_CENTRE_X, value=160.0))
    errors = outside_bounds_errors(build_all(), layout.interior_bounds())
    assert any(e.startswith("Microphone body spans X") for e in errors)
