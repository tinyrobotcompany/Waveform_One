from dataclasses import replace

from waveform_one import parameters as p
from waveform_one.validation import parameter_errors


def test_every_parameter_is_classified_and_set():
    params = p.all_params()
    assert params
    for name, param in params.items():
        assert isinstance(param.status, p.Status), name
        assert param.note, f"{name} needs a note explaining its source"
    assert parameter_errors(params) == []


def test_unset_parameter_is_rejected():
    params = p.all_params() | {"CASE_WIDTH": replace(p.CASE_WIDTH, value=None)}
    assert any("CASE_WIDTH is unset" in e for e in parameter_errors(params))


def test_non_positive_dimension_is_rejected():
    params = p.all_params() | {"CASE_DEPTH": replace(p.CASE_DEPTH, value=0.0)}
    assert any("CASE_DEPTH must be positive" in e for e in parameter_errors(params))


def test_print_limit_cannot_exceed_build_volume():
    params = p.all_params() | {"MAX_PRINT_Y": replace(p.MAX_PRINT_Y, value=300.0)}
    assert any("MAX_PRINT_Y (300.0) exceeds" in e for e in parameter_errors(params))
