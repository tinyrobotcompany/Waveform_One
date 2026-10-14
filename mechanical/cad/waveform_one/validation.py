"""Checks that must pass before any geometry is exported."""

from itertools import combinations

from build123d import Part, Shape

from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, PrintedPart, Vec3

OVERLAP_TOLERANCE_MM3 = 1e-3
BOUNDS_TOLERANCE_MM = 1e-6
PRINT_TOLERANCE_MM = 1e-3  # absorbs modelling-kernel tolerance on boolean results


class ValidationError(Exception):
    pass


def _layout_errors(params: dict[str, p.Param]) -> list[str]:
    """Design rules that relate several parameters."""

    def v(name: str) -> float:
        return params[name].value

    errors = []
    boss_r = v("BOSS_DIAMETER") / 2
    for band in ("TOP_BAND", "BOTTOM_BAND"):
        gap = v(band) / 2 - boss_r
        if gap < v("POCKET_CLEARANCE"):
            errors.append(f"{band} ({v(band):.1f} mm) leaves clip bosses {gap:.1f} mm from the components")
    groove_reach = v("TONGUE_DEPTH") + 2 * v("JOINT_CLEARANCE")
    if groove_reach > v("DIVIDER_WIDTH") / 2:
        errors.append(
            f"Shell groove reaches {groove_reach:.1f} mm from the seam, past the "
            f"{v('DIVIDER_WIDTH') / 2:.1f} mm half-divider into the LED window"
        )
    groove_t = v("TONGUE_THICKNESS") + 2 * v("JOINT_CLEARANCE")
    for host in ("SHELL_WALL", "FRONT_THICKNESS"):
        if groove_t >= v(host):
            errors.append(f"Shell groove ({groove_t:.2f} mm) leaves no material either side in {host} ({v(host):.1f} mm)")
    if v("CORNER_RADIUS") <= v("SHELL_WALL"):
        errors.append("CORNER_RADIUS must exceed SHELL_WALL so the inside corners stay rounded")
    width, layer = v("STRIPE_WIDTH"), v("LAYER_HEIGHT")
    first, second = v("STRIPE_1_FROM_FRONT"), v("STRIPE_2_FROM_FRONT")
    if first < v("FRONT_THICKNESS"):
        errors.append(f"The first colour stripe ({first:.1f} mm) starts inside the {v('FRONT_THICKNESS'):.1f} mm front plate")
    if second < first + width:
        errors.append(f"The colour stripes overlap: the second starts at {second:.1f} mm, before the first ends")
    if second + width > v("CASE_DEPTH"):
        errors.append(f"The second colour stripe runs past the {v('CASE_DEPTH'):.0f} mm case depth")
    for name, edge in (("STRIPE_1_FROM_FRONT", first), ("STRIPE_2_FROM_FRONT", second), ("STRIPE_WIDTH", width)):
        if abs(edge / layer - round(edge / layer)) > 1e-6:
            errors.append(f"{name} ({edge} mm) is not a whole number of {layer} mm layers")
    return errors


def parameter_errors(params: dict[str, p.Param], milestone: int = p.CURRENT_MILESTONE) -> list[str]:
    errors = []
    for name, param in params.items():
        if param.value is None:
            if param.required_from <= milestone:
                errors.append(f"{name} is unset ({param.status.value}): {param.note}")
        elif param.unit == "mm" and not param.signed and param.value <= 0:
            errors.append(f"{name} must be positive, got {param.value}")

    for axis in "XYZ":
        limit = params.get(f"MAX_PRINT_{axis}")
        build = params.get(f"PRINTER_BUILD_{axis}")
        if limit and build and limit.value and build.value and limit.value > build.value:
            errors.append(f"MAX_PRINT_{axis} ({limit.value}) exceeds PRINTER_BUILD_{axis} ({build.value})")

    if not errors:
        errors += _layout_errors(params)
    return errors


def require_valid_parameters(params: dict[str, p.Param] | None = None) -> None:
    errors = parameter_errors(p.all_params() if params is None else params)
    if errors:
        raise ValidationError("Invalid parameters:\n  " + "\n  ".join(errors))


def print_envelope_violations(shape: Shape) -> list[str]:
    """Why a part cannot be printed in its current orientation, if it cannot."""
    size = shape.bounding_box().size
    limits = (p.MAX_PRINT_X.v, p.MAX_PRINT_Y.v, p.MAX_PRINT_Z.v)
    return [
        f"{axis} extent {extent:.1f} mm exceeds {limit:.1f} mm print limit"
        for axis, extent, limit in zip("XYZ", (size.X, size.Y, size.Z), limits)
        if extent > limit + PRINT_TOLERANCE_MM
    ]


def _shapes(ph: Placeholder) -> dict[str, Part]:
    return {"body": ph.body} | {f"keep-out {name}": k for name, k in ph.keepouts.items()}


def outside_bounds_errors(placeholders: list[Placeholder], bounds: tuple[Vec3, Vec3]) -> list[str]:
    lo, hi = bounds
    errors = []
    for ph in placeholders:
        for what, shape in _shapes(ph).items():
            box = shape.bounding_box()
            for axis, low, high, limit_lo, limit_hi in zip("XYZ", box.min, box.max, lo, hi):
                if low < limit_lo - BOUNDS_TOLERANCE_MM or high > limit_hi + BOUNDS_TOLERANCE_MM:
                    errors.append(
                        f"{ph.name} {what} spans {axis} {low:.1f}..{high:.1f}, "
                        f"outside interior {limit_lo:.1f}..{limit_hi:.1f}"
                    )
    return errors


def _overlap_volume(a: Part, b: Part) -> float:
    if not a.bounding_box().overlaps(b.bounding_box()):
        return 0.0
    common = a & b
    return common.volume if common else 0.0


def collision_errors(placeholders: list[Placeholder]) -> list[str]:
    """Bodies must not touch other bodies or other components' keep-outs."""
    errors = []
    for a, b in combinations(placeholders, 2):
        pairs = [(f"{a.name} body", a.body, f"{b.name} body", b.body)]
        pairs += [(f"{a.name} body", a.body, f"{b.name} keep-out {n}", k) for n, k in b.keepouts.items()]
        pairs += [(f"{b.name} body", b.body, f"{a.name} keep-out {n}", k) for n, k in a.keepouts.items()]
        for name_a, shape_a, name_b, shape_b in pairs:
            volume = _overlap_volume(shape_a, shape_b)
            if volume > OVERLAP_TOLERANCE_MM3:
                errors.append(f"{name_a} overlaps {name_b} by {volume:.1f} mm³")
    return errors


def require_valid_layout(placeholders: list[Placeholder], bounds: tuple[Vec3, Vec3] | None) -> None:
    """Unique names, no collisions and, when bounds are given, everything inside them."""
    names = [ph.name for ph in placeholders]
    errors = [f"Duplicate component name {n}" for n in set(names) if names.count(n) > 1]
    if bounds is not None:
        errors += outside_bounds_errors(placeholders, bounds)
    errors += collision_errors(placeholders)
    if errors:
        raise ValidationError("Invalid layout:\n  " + "\n  ".join(errors))


def require_printable(parts: list[PrintedPart]) -> None:
    """Every part must fit the engineering print envelope in its print orientation."""
    errors = [f"{part.name}: {v}" for part in parts for v in print_envelope_violations(part.on_bed())]
    if errors:
        raise ValidationError("Parts exceed the print envelope:\n  " + "\n  ".join(errors))
