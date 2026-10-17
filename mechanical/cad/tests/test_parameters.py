import ast
from dataclasses import replace
from pathlib import Path

import pytest

from waveform_one import parameters as p
from waveform_one.validation import parameter_errors

PACKAGE = Path(p.__file__).parent


def with_value(name, value):
    return p.all_params() | {name: replace(p.all_params()[name], value=value)}


def test_every_parameter_is_classified_and_valid():
    params = p.all_params()
    for name, param in params.items():
        assert isinstance(param.status, p.Status), name
        assert param.note, f"{name} needs a note explaining its source"
    assert parameter_errors(params) == []


def test_owner_measurements_are_marked_measured():
    for name in ("SCREEN_W", "SCREEN_H"):
        assert p.all_params()[name].status is p.Status.MEASURED


def test_led_panel_follows_the_waveshare_drawing():
    assert (p.LED_W.v, p.LED_H.v, p.LED_D.v) == (191.8, 95.8, 10.0)
    for name in ("LED_W", "LED_H", "LED_D"):
        assert p.all_params()[name].status is p.Status.DATASHEET


def test_insert_hole_is_required_for_the_enclosure():
    errors = parameter_errors(with_value("M3_INSERT_HOLE", None))
    assert any("M3_INSERT_HOLE is unset" in e for e in errors)
    assert parameter_errors(with_value("M3_INSERT_HOLE", None), milestone=4) == []


def test_non_positive_dimension_is_rejected():
    assert any("CASE_DEPTH must be positive" in e for e in parameter_errors(with_value("CASE_DEPTH", 0.0)))


def test_offsets_may_be_negative():
    assert parameter_errors(with_value("SCREEN_ACTIVE_OFFSET_X", -2.0)) == []


def test_print_limit_cannot_exceed_build_volume():
    assert any("MAX_PRINT_Y (300.0) exceeds" in e for e in parameter_errors(with_value("MAX_PRINT_Y", 300.0)))


def test_band_too_narrow_for_clip_bosses_fails_clearly():
    errors = parameter_errors(with_value("TOP_BAND", 8.0))
    assert "TOP_BAND (8.0 mm) leaves clip bosses 0.0 mm from the components" in errors


def test_divider_too_narrow_for_the_shell_joint_fails_clearly():
    errors = parameter_errors(with_value("DIVIDER_WIDTH", 8.0))
    assert len(errors) == 1 and "Shell groove reaches 5.5 mm" in errors[0]


GEOMETRY_MODULES = (
    sorted(PACKAGE.glob("components/*.py"))
    + sorted(PACKAGE.glob("enclosure/*.py"))
    + sorted(PACKAGE.glob("calibration/*.py"))
    + [PACKAGE / "layout.py"]
)


@pytest.mark.parametrize("module", GEOMETRY_MODULES, ids=lambda m: m.name)
def test_geometry_modules_have_no_dimensional_literals(module):
    """Dimensions come from parameters.py; colours, 0, 1, 2, the marker size and quarter turns are allowed."""
    allowed = {0, 0.0, 1, 2, 0.2, 90}
    tree = ast.parse(module.read_text())
    colour_args = {
        id(arg)
        for call in ast.walk(tree)
        if isinstance(call, ast.Call) and getattr(call.func, "id", None) == "Color"
        for arg in call.args
    }
    literals = [
        node.value
        for node in ast.walk(tree)
        if isinstance(node, ast.Constant)
        and isinstance(node.value, (int, float))
        and not isinstance(node.value, bool)
        and id(node) not in colour_args
    ]
    assert [v for v in literals if v not in allowed] == []


def test_tongue_too_thick_for_the_wall_fails_clearly():
    errors = parameter_errors(with_value("TONGUE_THICKNESS", 3.6))
    assert any("leaves no material either side in SHELL_WALL" in e for e in errors)


def test_overlapping_colour_stripes_fail_clearly():
    errors = parameter_errors(with_value("STRIPE_2_FROM_FRONT", 15.0))
    assert any("The colour stripes overlap" in e for e in errors)


def test_colour_stripe_edges_must_fall_on_layer_boundaries():
    errors = parameter_errors(with_value("STRIPE_WIDTH", 10.1))
    assert any("STRIPE_WIDTH (10.1 mm) is not a whole number of 0.2 mm layers" in e for e in errors)


def test_colour_stripe_cannot_start_inside_the_front_plate():
    errors = parameter_errors(with_value("STRIPE_1_FROM_FRONT", 2.0))
    assert any("starts inside the 4.0 mm front plate" in e for e in errors)


def test_magnet_too_thick_for_the_top_wall_fails_clearly():
    errors = parameter_errors(with_value("MAGNET_THICKNESS", 3.5))
    assert any("Holder magnet pockets leave 0.3 mm of top wall" in e for e in errors)
