import math

import pytest
from build123d import import_stl

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.build import build_enclosure_parts
from waveform_one.calibration.coupon import build_coupon
from waveform_one.components import build_all, lcd
from waveform_one import carrier
from waveform_one.enclosure import clips, rear_panel, record_holder, shell
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
    assert names[-4:] == ["rear_panel_left", "rear_panel_right", "record_holder_left", "record_holder_right"]


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


def test_name_is_centred_on_the_strip_below_the_led_window():
    (lx0, lx1), (lz0, _) = shell.led_window()
    box = shell.brand_name().bounding_box()
    assert box.center().X == pytest.approx((lx0 + lx1) / 2, abs=1.0)  # glyph shapes shift it slightly
    visible_edge = lz0 - p.WINDOW_CHAMFER.v
    # Equal plastic above and below the lettering.
    assert box.min.Z == pytest.approx(visible_edge - box.max.Z, abs=0.5)


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


def test_carrier_board_sits_on_the_flat_floor_of_one_shell_half():
    (x0, x1), (y0, y1) = carrier.bounds()
    (x_min, y_min, _), (x_max, y_max, _) = layout.interior_bounds()
    flat = p.CORNER_RADIUS.v - p.SHELL_WALL.v
    assert x_min + flat <= x0 and x1 <= x_max - flat
    assert y_min < y0 and y1 < y_max
    assert x0 > layout.seam_x() + p.TONGUE_DEPTH.v  # entirely in the right half
    assert carrier.bottom_z() - layout.interior_bounds()[0][2] >= p.CARRIER_MIN_STANDOFF.v


def test_carrier_posts_line_up_with_the_board_holes(parts):
    board = carrier.board()
    for x, y in carrier.hole_centres():
        assert carrier.bounds()[0][0] < x < carrier.bounds()[0][1]
        assert carrier.bounds()[1][0] < y < carrier.bounds()[1][1]
    # Each post's insert hole is in the shell under its board hole.
    shell_right = parts["shell_right"].body
    (_, _, z_floor), _ = layout.interior_bounds()
    top = z_floor + board["standoff_height"]
    from build123d import Pos, Box
    for x, y in carrier.hole_centres():
        probe = Pos(x, y, top - 1.0) * Box(0.5, 0.5, 0.5)
        assert (shell_right & probe).volume == pytest.approx(0, abs=1e-6)  # hole is open
        ring = Pos(x + p.M3_INSERT_HOLE.v / 2 + 1.0, y, top - 1.0) * Box(0.5, 0.5, 0.5)
        assert (shell_right & ring).volume > 0  # post material around it


def test_rear_panel_has_an_opening_at_each_rear_socket(parts):
    panel = parts["rear_panel_left"].body + parts["rear_panel_right"].body
    (_, _, _), (_, y_inner, _) = layout.interior_bounds()
    from build123d import Pos, Box
    openings = carrier.rear_openings()
    assert {ref for ref, *_ in openings} == {"J1", "J2"}
    for _, x0, x1, z0, z1 in openings:
        probe = Pos((x0 + x1) / 2, y_inner + p.REAR_PANEL_THICKNESS.v / 2, (z0 + z1) / 2) * Box(1, 1, 1)
        assert (panel & probe).volume == pytest.approx(0, abs=1e-6)
        assert carrier.board()["case_rear_y"] < y_inner  # socket faces sit inside the panel


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


def test_shell_halves_split_into_a_black_body_and_two_colour_stripes(parts):
    for name in ("shell_left", "shell_right"):
        part = parts[name]
        expected = [(f"{name}_body", 1), (f"{name}_stripe_1", 2), (f"{name}_stripe_2", 3)]
        if name == "shell_right":  # the name sits under the LED panel
            expected.append(("shell_right_name", p.NAME_FILAMENT))
        assert [(b.name, b.filament) for b in part.bands] == expected
        # Together the bands are exactly the shell half.
        assert sum(b.body.volume for b in part.bands) == pytest.approx(part.body.volume, rel=1e-6)
        for band, (y0, y1) in zip(part.bands[1:], shell.stripe_slabs()):
            box = band.body.bounding_box()
            assert (box.min.Y, box.max.Y) == pytest.approx((y0, y1), abs=1e-6), band.name


def test_colour_stripes_are_whole_layers_on_the_bed(parts):
    """Front face down, stripe depth is print height: 10-20 mm and 30-40 mm up."""
    for name in ("shell_left", "shell_right"):
        bed = parts[name].bands_on_bed()
        assert bed[0].body.bounding_box().min.Z == pytest.approx(0.0, abs=1e-6)
        for band, start in zip(bed[1:], (p.STRIPE_1_FROM_FRONT.v, p.STRIPE_2_FROM_FRONT.v)):
            box = band.body.bounding_box()
            assert (box.min.Z, box.max.Z) == pytest.approx((start, start + p.STRIPE_WIDTH.v), abs=1e-6)


def _read_3mf(path):
    import xml.etree.ElementTree as ET
    import zipfile

    ns = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"}
    with zipfile.ZipFile(path) as package:
        model = ET.fromstring(package.read("3D/3dmodel.model"))
        config = ET.fromstring(package.read("Metadata/model_settings.config"))
    return model, config, ns


def _signed_volume(mesh, ns):
    import numpy as np

    points = np.array([[float(v.get(a)) for a in "xyz"] for v in mesh.find("m:vertices", ns)])
    triangles = np.array([[int(t.get(k)) for k in ("v1", "v2", "v3")] for t in mesh.find("m:triangles", ns)])
    a, b, c = points[triangles[:, 0]], points[triangles[:, 1]], points[triangles[:, 2]]
    return float(np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6)


def _open_edges(mesh, ns):
    """Edges not shared by exactly two triangles, each way round once: slicers report them as non-manifold."""
    from collections import Counter

    edges = Counter()
    for t in mesh.find("m:triangles", ns):
        a, b, c = (int(t.get(k)) for k in ("v1", "v2", "v3"))
        edges.update([(a, b), (b, c), (c, a)])
    return sum(1 for (a, b), n in edges.items() if n != 1 or edges[(b, a)] != 1)


def test_shell_3mf_is_one_object_of_three_parts_with_their_filaments(parts, tmp_path):
    part = parts["shell_left"]
    export_printed_parts([part], tmp_path)
    model, config, ns = _read_3mf(tmp_path / "3mf" / f"{printed_part_stem(part)}.3mf")
    objects = {o.get("id"): o for o in model.find("m:resources", ns)}
    (item,) = model.find("m:build", ns)
    parent = objects[item.get("objectid")]
    assert parent.get("name") == "shell_left"
    meshes = [objects[c.get("objectid")] for c in parent.find("m:components", ns)]
    assert [m.get("name") for m in meshes] == [b.name for b in part.bands]
    # Outward-facing triangles: each mesh encloses its band's volume.
    for mesh, band in zip(meshes, part.bands):
        assert _signed_volume(mesh.find("m:mesh", ns), ns) == pytest.approx(band.body.volume, rel=1e-3), band.name
        assert _open_edges(mesh.find("m:mesh", ns), ns) == 0, band.name
    filaments = {pt.find("metadata[@key='name']").get("value"): pt.find("metadata[@key='extruder']").get("value")
                 for pt in config.iter("part")}
    assert filaments == {b.name: str(b.filament) for b in part.bands}


def test_name_is_a_flush_white_inlay_in_the_first_layers(parts):
    """The lettering is cut into the front face and filled, so the front prints flat."""
    right = parts["shell_right"]
    name = right.bands_on_bed()[-1]
    box = name.body.bounding_box()
    assert name.filament == p.NAME_FILAMENT
    assert (box.min.Z, box.max.Z) == pytest.approx((0.0, p.BRAND_DEPTH.v), abs=1e-6)
    assert name.body.volume == pytest.approx(shell.brand_name().volume, rel=1e-6)
    # Filled: the bed face of the shell has no recess where the name is.
    assert right.on_bed().bounding_box().min.Z == pytest.approx(0.0, abs=1e-6)
    assert all(b.filament != p.NAME_FILAMENT for b in parts["shell_left"].bands)
