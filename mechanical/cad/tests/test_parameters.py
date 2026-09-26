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


def test_unset_parameter_is_rejected_once_its_milestone_arrives():
    assert p.M3_INSERT_HOLE.value is None
    assert parameter_errors(p.all_params(), milestone=4) == []
    errors = parameter_errors(p.all_params(), milestone=5)
    assert any("M3_INSERT_HOLE is unset" in e for e in errors)


def test_unset_current_parameter_is_rejected():
    assert any("CASE_WIDTH is unset" in e for e in parameter_errors(with_value("CASE_WIDTH", None)))


def test_non_positive_dimension_is_rejected():
    assert any("CASE_DEPTH must be positive" in e for e in parameter_errors(with_value("CASE_DEPTH", 0.0)))


def test_offsets_may_be_negative():
    assert parameter_errors(with_value("DISPLAY_ACTIVE_OFFSET_X", -2.0)) == []


def test_print_limit_cannot_exceed_build_volume():
    assert any("MAX_PRINT_Y (300.0) exceeds" in e for e in parameter_errors(with_value("MAX_PRINT_Y", 300.0)))


def test_case_too_short_for_front_stack_fails_clearly():
    errors = parameter_errors(with_value("CASE_HEIGHT", 220.0))
    assert len(errors) == 1 and "Front stack reaches 232.0 mm" in errors[0]


def test_case_too_narrow_for_front_components_fails_clearly():
    errors = parameter_errors(with_value("CASE_WIDTH", 205.0))
    assert len(errors) == 1 and "Widest front component" in errors[0]


@pytest.mark.parametrize("module", sorted(PACKAGE.glob("components/*.py")) + [PACKAGE / "layout.py"])
def test_geometry_modules_have_no_dimensional_literals(module):
    """Dimensions come from parameters.py; colours and 0, 1, 2 and the marker size are allowed."""
    allowed = {0, 0.0, 1, 2, 0.2}
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
