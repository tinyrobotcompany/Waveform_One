import math

import pytest
from build123d import import_stl

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.build import build_enclosure_parts
from waveform_one.calibration.coupon import build_coupon
from waveform_one.components import build_all, lcd
from waveform_one.enclosure import clips, pi_tray, rear_panel, record_holder, shell
from waveform_one.export import export_printed_parts, printed_part_stem
from waveform_one.geometry import Placeholder
from waveform_one.validation import collision_errors, print_envelope_violations

TOL = 1e-6


@pytest.fixture(scope="module")
def parts():
    return {pp.name: pp for pp in build_enclosure_parts()}


def test_expected_parts(parts):
    names = list(parts)
    assert names[:2] == ["shell_left", "shell_right"]
    assert sum(n.startswith("clip_") for n in names) == 8
    assert names[-5:] == ["pi_tray", "rear_panel_left", "rear_panel_right", "record_holder_left", "record_holder_right"]


def test_every_part_fits_the_printer_lying_on_the_bed(parts):
    for part in list(parts.values()) + [build_coupon()]:
        bed = part.on_bed()
        assert bed.bounding_box().min.Z == pytest.approx(0.0, abs=1e-6), part.name
        assert print_envelope_violations(bed) == [], part.name


def test_each_shell_half_is_one_solid_printed_front_down(parts):
    for name in ("shell_left", "shell_right"):
        body = parts[name].body
        assert len(body.solids()) == 1
        assert parts[name].on_bed().bounding_box().size.Z == pytest.approx(p.CASE_DEPTH.v, abs=1e-3)


def test_shell_splits_at_the_divider_with_a_tongue(parts):
    seam = layout.seam_x()
    left, right = parts["shell_left"].body.bounding_box(), parts["shell_right"].body.bounding_box()
    assert left.max.X == pytest.approx(seam + p.TONGUE_DEPTH.v)
    assert right.min.X == pytest.approx(seam, abs=1e-3)
    assert (left.min.X, right.max.X) == pytest.approx((-layout.case_width() / 2, layout.case_width() / 2))


def test_tongue_slides_into_groove_without_touching(parts):
    common = parts["shell_left"].body & parts["shell_right"].body
    assert (common.volume if common else 0.0) == pytest.approx(0.0, abs=1e-6)


def test_screen_window_shows_the_whole_visible_area():
    (wx0, wx1), (wz0, wz1) = shell.screen_window()
    (ax0, ax1), (az0, az1) = lcd.active_area()
    assert wx0 < ax0 and ax1 < wx1 and wz0 < az0 and az1 < wz1


def test_windows_stay_clear_of_the_seam():
    seam = layout.seam_x()
    assert shell.screen_window()[0][1] < seam - p.TONGUE_DEPTH.v
    assert shell.led_window()[0][0] > seam + p.TONGUE_DEPTH.v + 2 * p.JOINT_CLEARANCE.v


def test_led_window_hides_the_panel_edge_behind_a_lip():
    (x0, x1), (z0, z1) = shell.led_window()
    assert x1 - x0 == pytest.approx(p.LED_W.v - 2 * p.LED_WINDOW_LIP.v)
    assert z1 - z0 == pytest.approx(p.LED_H.v - 2 * p.LED_WINDOW_LIP.v)


def test_name_sits_under_the_led_window_inside_the_frame():
    x, z = shell.brand_centre()
    (lx0, lx1), (lz0, _) = shell.led_window()
    assert lx0 < x < lx1
    assert p.FRAME_GROOVE_INSET.v + p.FRAME_GROOVE_WIDTH.v < z < lz0


def test_four_clips_per_component_each_over_a_component_edge(parts):
    sites = clips.clip_sites()
    assert len(sites) == 8
    for site in sites:
        clip = parts[site.name].body.bounding_box()
        assert clip.min.Y == pytest.approx(site.rear_y)
        assert clip.min.Z < site.edge_z < clip.max.Z
        reach = abs(site.edge_z - (clip.max.Z if site.boss_z < site.edge_z else clip.min.Z))
        assert reach == pytest.approx(p.CLIP_OVERLAP.v)


def test_rear_panel_halves_bridge_the_shell_seam(parts):
    seam = layout.seam_x()
    left = parts["rear_panel_left"].body.bounding_box()
    assert left.min.X < seam < left.max.X
    assert left.max.X == pytest.approx(p.REAR_SEAM_X.v, abs=1e-3)


def test_rear_panel_screws_line_up_with_wall_bosses():
    (_, _, z_lo), (_, _, z_hi) = layout.interior_bounds()
    for x, z in rear_panel.boss_sites():
        assert z_lo < z < z_hi
        assert abs(x - layout.seam_x()) > p.REAR_BOSS_WIDTH.v / 2 + p.TONGUE_DEPTH.v
        assert abs(x - p.REAR_SEAM_X.v) > p.REAR_BOSS_WIDTH.v / 2


def test_pi_tray_stays_on_the_flat_floor():
    (x0, _), _ = pi_tray.tray_bounds()
    (x_min, _, _), _ = layout.interior_bounds()
    assert x0 >= x_min + p.CORNER_RADIUS.v - p.SHELL_WALL.v - TOL


def test_holders_lean_the_sleeve_back_from_their_slots(parts):
    y, z = record_holder.slot_bottom()
    assert z == pytest.approx(layout.case_height() + p.HOLDER_HEIGHT.v - p.RECORD_SLOT_DEPTH.v)
    for side in ("left", "right"):
        box = parts[f"record_holder_{side}"].body.bounding_box()
        assert box.min.Z == pytest.approx(layout.case_height())
        assert box.min.Y < y < box.max.Y
    lean = p.RECORD_SLOT_DEPTH.v * math.tan(math.radians(p.RECORD_ANGLE_DEG.v))
    assert lean > 0  # the slot's top is further back than its floor


def test_holder_screws_pass_through_the_top_wall():
    for x, y in record_holder.screw_positions():
        assert abs(x) < layout.case_width() / 2 - p.CORNER_RADIUS.v
        assert 0 < y < p.CASE_DEPTH.v


def test_enclosure_parts_do_not_hit_electronics_or_each_other(parts):
    as_placeholders = [Placeholder(pp.name, pp.body, None) for pp in parts.values()]
    assert collision_errors(build_all() + as_placeholders) == []


def test_printed_part_files_are_named_and_on_the_bed(tmp_path):
    coupon = build_coupon()
    written = export_printed_parts([coupon], tmp_path)
    stem = printed_part_stem(coupon)
    assert stem == f"calibration_coupon_rev{p.PRINT_REVISION}_{p.PRINT_MATERIAL}"
    assert sorted(path.name for path in written) == [f"{stem}.3mf", f"{stem}.stl"]
    stl = import_stl(tmp_path / "stl" / f"{stem}.stl").bounding_box()
    assert stl.min.Z == pytest.approx(0.0, abs=1e-3)
    assert stl.size.Z == pytest.approx(p.COUPON_THICKNESS.v, abs=1e-3)


def test_coupon_has_every_trial_hole():
    coupon = build_coupon().body
    expected = len(p.COUPON_INSERT_HOLES) + len(p.COUPON_CLEARANCE_HOLES)
    assert len(coupon.faces().filter_by(lambda f: f.geom_type.name == "CYLINDER")) == expected
