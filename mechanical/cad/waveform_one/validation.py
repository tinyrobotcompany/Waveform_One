"""Checks that must pass before any geometry is exported."""

from build123d import Shape

from waveform_one import parameters as p


class ValidationError(Exception):
    pass


def parameter_errors(params: dict[str, p.Param]) -> list[str]:
    errors = []
    for name, param in params.items():
        if param.value is None:
            errors.append(f"{name} is unset ({param.status.value}): {param.note}")
        elif param.unit == "mm" and param.value <= 0:
            errors.append(f"{name} must be positive, got {param.value}")

    for axis in "XYZ":
        limit = params.get(f"MAX_PRINT_{axis}")
        build = params.get(f"PRINTER_BUILD_{axis}")
        if limit and build and limit.value and build.value and limit.value > build.value:
            errors.append(f"MAX_PRINT_{axis} ({limit.value}) exceeds PRINTER_BUILD_{axis} ({build.value})")
    return errors


def require_valid_parameters(params: dict[str, p.Param] | None = None) -> None:
    errors = parameter_errors(p.all_params() if params is None else params)
    if errors:
        raise ValidationError("Invalid parameters:\n  " + "\n  ".join(errors))


def print_envelope_violations(shape: Shape) -> list[str]:
    """Why a part cannot be printed in its current orientation, if it cannot."""
    size = shape.bounding_box().size
    limits = (p.MAX_PRINT_X.value, p.MAX_PRINT_Y.value, p.MAX_PRINT_Z.value)
    return [
        f"{axis} extent {extent:.1f} mm exceeds {limit:.1f} mm print limit"
        for axis, extent, limit in zip("XYZ", (size.X, size.Y, size.Z), limits)
        if extent > limit
    ]
