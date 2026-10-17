from dataclasses import replace

import pytest

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one import carrier
from waveform_one.components import build_all
from waveform_one.enclosure import clips
from waveform_one.validation import ValidationError, collision_errors, outside_bounds_errors, require_valid_layout

EXPECTED = ["Screen", "Diffuser", "LedPanel", "CarrierBoard", "LedAdapter", "Microphone"]


@pytest.fixture(scope="module")
def parts():
    return {ph.name: ph for ph in build_all()}


def box(parts, name):
    return parts[name].body.bounding_box()


def test_case_size_follows_components_and_bands():
    width, depth, height = layout.case_size()
    assert width == pytest.approx(
        2 * (p.SHELL_WALL.v + p.SIDE_BAND.v) + p.SCREEN_W.v + p.DIVIDER_WIDTH.v + p.LED_W.v
    )
    assert height == pytest.approx(2 * p.SHELL_WALL.v + p.TOP_BAND.v + p.BOTTOM_BAND.v + max(p.SCREEN_H.v, p.LED_H.v))
    assert depth == p.CASE_DEPTH.v


def test_each_component_exists_once(parts):
    assert list(parts) == EXPECTED


def test_everything_fits_inside_the_walls(parts):
    assert outside_bounds_errors(list(parts.values()), layout.interior_bounds()) == []


def test_no_component_collides_or_blocks_a_keepout(parts):
    assert collision_errors(list(parts.values())) == []


def test_screen_left_led_right_with_the_seam_in_the_divider(parts):
    screen, led = box(parts, "Screen"), box(parts, "LedPanel")
    assert screen.max.X < layout.seam_x() < led.min.X
    assert led.min.X - screen.max.X == pytest.approx(p.DIVIDER_WIDTH.v)
    assert layout.seam_x() == pytest.approx((screen.max.X + led.min.X) / 2)


def test_screen_and_led_share_a_vertical_centre(parts):
    centre = layout.zone_centre_z()
    for name in ("Screen", "LedPanel"):
        assert box(parts, name).center().Z == pytest.approx(centre)


def test_front_components_sit_against_the_front_plate(parts):
    assert box(parts, "Screen").min.Y == pytest.approx(p.FRONT_THICKNESS.v)
    assert box(parts, "Diffuser").min.Y == pytest.approx(p.FRONT_THICKNESS.v)
    assert box(parts, "LedPanel").min.Y == pytest.approx(box(parts, "Diffuser").max.Y)


def test_components_use_the_owner_measurements_and_the_led_drawing(parts):
    screen, led = box(parts, "Screen").size, box(parts, "LedPanel").size
    assert (screen.X, screen.Z) == pytest.approx((165.0, 100.0))
    assert (led.X, led.Y, led.Z) == pytest.approx((191.8, 10.0, 95.8))


def test_led_clips_press_on_the_back_of_the_panel(parts):
    panel_back = box(parts, "LedPanel").max.Y
    for site in clips.clip_sites():
        if site.name.startswith("clip_led_"):
            assert site.rear_y == pytest.approx(panel_back)


def test_carrier_board_follows_the_kicad_placement():
    holes = carrier.hole_centres()
    xs, ys = sorted({x for x, _ in holes}), sorted({y for _, y in holes})
    board = carrier.board()
    assert len(holes) == 4
    assert xs[0] == pytest.approx(board["case_left_x"] + 4.0)
    assert ys[1] == pytest.approx(board["case_rear_y"] - 4.0)
    assert {item["ref"] for item in carrier.parts()} >= {"J1", "J3", "J4", "J5", "J6", "J7", "U1"}


def test_led_adapter_follows_its_kicad_placement():
    from waveform_one.pcb import LED_ADAPTER
    holes = LED_ADAPTER.hole_centres()
    board = LED_ADAPTER.board()
    assert len(holes) == 4
    assert min(x for x, _ in holes) == pytest.approx(board["case_left_x"] + 4.0)
    assert max(y for _, y in holes) == pytest.approx(board["case_rear_y"] - 4.0)
    assert {item["ref"] for item in LED_ADAPTER.parts()} >= {"J1", "J2", "J3", "J4", "U1", "U2"}


def test_microphone_sits_on_the_rear_panel(parts):
    _, hi = layout.interior_bounds()
    assert box(parts, "Microphone").max.Y == pytest.approx(hi[1])


def test_carrier_cables_pushed_into_the_led_panel_are_caught(monkeypatch):
    monkeypatch.setattr(p, "CARRIER_WIRE_KEEPOUT", replace(p.CARRIER_WIRE_KEEPOUT, value=60.0))
    with pytest.raises(ValidationError, match="CarrierBoard keep-out LedPowerWires"):
        require_valid_layout(build_all(), layout.interior_bounds())


def test_component_pushed_through_a_wall_is_caught(monkeypatch):
    monkeypatch.setattr(p, "MIC_CENTRE_X", replace(p.MIC_CENTRE_X, value=200.0))
    errors = outside_bounds_errors(build_all(), layout.interior_bounds())
    assert any(e.startswith("Microphone body spans X") for e in errors)
