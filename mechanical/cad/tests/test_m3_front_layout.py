import pytest
from build123d import import_stl

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import build_all as build_electronics
from waveform_one.enclosure import front_panel
from waveform_one.export import export_printed_parts, printed_part_stem
from waveform_one.geometry import Placeholder
from waveform_one.validation import collision_errors, print_envelope_violations

TOL = 1e-6


@pytest.fixture(scope="module")
def parts():
    return {pp.name: pp for pp in front_panel.build_all()}


def test_expected_printable_parts(parts):
    assert list(parts) == [
        "front_left",
        "front_center",
        "front_right",
        "front_splice_left",
        "front_splice_right",
        "stand_left",
        "stand_right",
    ]


def test_every_part_fits_the_print_envelope_on_the_bed(parts):
    for part in parts.values():
        bed = part.on_bed()
        assert bed.bounding_box().min.Z == pytest.approx(0.0, abs=TOL), part.name
        assert print_envelope_violations(bed) == [], part.name


def test_panel_pieces_lie_flat_front_face_down(parts):
    for name in ("front_left", "front_center", "front_right"):
        assert parts[name].on_bed().bounding_box().size.Z == pytest.approx(p.FRONT_BEZEL_THICKNESS.v)


def test_pieces_reassemble_into_the_full_panel(parts):
    pieces = sum(parts[n].body.volume for n in ("front_left", "front_center", "front_right"))
    assert pieces == pytest.approx(front_panel.build_full_panel().volume, rel=1e-9)
    box = front_panel.build_full_panel().bounding_box()
    assert (box.size.X, box.size.Z) == pytest.approx((p.CASE_WIDTH.v, p.CASE_HEIGHT.v))


def test_seams_stay_clear_of_both_windows_and_components():
    s = front_panel.seam_x()
    for (x0, x1), _ in (front_panel.display_window(), front_panel.matrix_window()):
        assert -s < x0 and x1 < s
    assert s >= max(p.DISPLAY_W.v, p.MATRIX_W.v) / 2 + p.FRONT_SEAM_MARGIN.v - TOL
    assert p.CASE_WIDTH.v / 2 - s >= p.MIN_SIDE_PIECE_WIDTH.v


def test_display_window_frames_the_visible_area():
    (x0, x1), (z0, z1) = front_panel.display_window()
    margin = p.DISPLAY_WINDOW_MARGIN.v
    assert x1 - x0 == pytest.approx(p.DISPLAY_ACTIVE_W.v + 2 * margin)
    assert z1 - z0 == pytest.approx(p.DISPLAY_ACTIVE_H.v + 2 * margin)
    _, _, dz = layout.display_origin()
    assert (z0 + z1) / 2 == pytest.approx(dz + p.DISPLAY_H.v / 2)


def test_matrix_window_hides_the_panel_edge_behind_a_lip():
    (x0, x1), (z0, z1) = front_panel.matrix_window()
    lip = p.MATRIX_WINDOW_LIP.v
    assert x1 - x0 == pytest.approx(p.MATRIX_W.v - 2 * lip)
    assert z1 - z0 == pytest.approx(p.MATRIX_H.v - 2 * lip)


def test_window_edges_are_chamfered():
    panel = front_panel.build_full_panel()
    t = p.FRONT_BEZEL_THICKNESS.v
    windows = [front_panel.display_window(), front_panel.matrix_window()]
    square_cut_volume = p.CASE_WIDTH.v * p.CASE_HEIGHT.v * t - sum(
        (x1 - x0) * (z1 - z0) * t for (x0, x1), (z0, z1) in windows
    )
    c = p.WINDOW_CHAMFER.v
    chamfer_volume = sum(2 * ((x1 - x0) + (z1 - z0)) * c * c / 2 for (x0, x1), (z0, z1) in windows)
    assert panel.volume == pytest.approx(square_cut_volume - chamfer_volume, rel=1e-3)


def test_stands_hold_the_panel_with_clearance(parts):
    stand = parts["stand_left"]
    assert stand.mockup_only
    panel = parts["front_left"].body
    assert (stand.body & panel).volume == pytest.approx(0.0, abs=1e-6)


def test_panel_and_splices_do_not_hit_any_electronics(parts):
    in_enclosure = [Placeholder(pp.name, pp.body, None) for pp in parts.values() if not pp.mockup_only]
    assert len(in_enclosure) == 5
    assert collision_errors(build_electronics() + in_enclosure) == []


def test_printed_part_files_are_named_and_on_the_bed(tmp_path, parts):
    center = parts["front_center"]
    written = export_printed_parts([center], tmp_path)
    stem = printed_part_stem(center)
    assert stem == f"front_center_rev{p.PRINT_REVISION}_{p.PRINT_MATERIAL}"
    assert sorted(path.name for path in written) == [f"{stem}.3mf", f"{stem}.stl"]
    stl = import_stl(tmp_path / "stl" / f"{stem}.stl").bounding_box()
    assert stl.min.Z == pytest.approx(0.0, abs=1e-3)
    assert stl.size.Z == pytest.approx(p.FRONT_BEZEL_THICKNESS.v, abs=1e-3)


def test_brand_text_is_engraved_into_the_side_strips_only():
    s = front_panel.seam_x()
    left, right = front_panel.build_brand_cuts()
    for cut, sign in ((left, -1), (right, 1)):
        box = cut.bounding_box()
        assert box.min.Y == pytest.approx(0.0, abs=1e-6)
        assert box.max.Y == pytest.approx(p.BRAND_DEPTH.v, abs=1e-6)
        inner, outer = sorted((sign * s, sign * p.CASE_WIDTH.v / 2))
        assert inner + p.BRAND_SIDE_MARGIN.v - 1e-6 <= box.min.X
        assert box.max.X <= outer - p.BRAND_SIDE_MARGIN.v + 1e-6
    assert left.bounding_box().max.Z == pytest.approx(front_panel.display_window()[1][1])


def test_brand_text_is_legible_and_left_label_is_the_widest_fit():
    size = front_panel.brand_font_size()
    left, _ = front_panel.build_brand_cuts()
    assert left.bounding_box().size.Z >= p.BRAND_MIN_CAP_HEIGHT.v
    available = p.CASE_WIDTH.v / 2 - front_panel.seam_x() - 2 * p.BRAND_SIDE_MARGIN.v
    assert left.bounding_box().size.X == pytest.approx(available, rel=1e-3) or size == p.BRAND_MAX_FONT_SIZE.v


def test_engraving_leaves_the_centre_piece_untouched(parts):
    centre = parts["front_center"].body
    for cut in front_panel.build_brand_cuts():
        assert (centre & cut).volume == pytest.approx(0.0, abs=1e-6)


def test_too_long_text_fails_clearly(monkeypatch):
    from waveform_one.validation import ValidationError

    monkeypatch.setattr(p, "BRAND_TEXT_LEFT", "WAVEFORM ONE HIGH FIDELITY VISUALISER")
    with pytest.raises(ValidationError, match="below BRAND_MIN_CAP_HEIGHT"):
        front_panel.build_brand_cuts()
