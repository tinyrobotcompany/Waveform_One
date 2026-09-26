"""Write generated geometry to mechanical/exports/.

Generated files are never written next to source code. Filenames are stable so
a rebuild overwrites the previous output instead of accumulating copies.
"""

from pathlib import Path

from build123d import Compound, ExportSVG, LineType, export_step

EXPORT_ROOT = Path(__file__).resolve().parents[2] / "exports"
REVISION = "m0"

# Isometric-style view from front-left-above (front is -Y, left is -X).
PREVIEW_CAMERA = (-700.0, -900.0, 600.0)
PREVIEW_UP = (0.0, 0.0, 1.0)


def reference_step_path(root: Path) -> Path:
    return root / "step" / f"waveform_one_{REVISION}_reference.step"


def preview_svg_path(root: Path) -> Path:
    return root / "previews" / f"waveform_one_{REVISION}_iso.svg"


def export_model(model: Compound, root: Path) -> list[Path]:
    step_path = reference_step_path(root)
    step_path.parent.mkdir(parents=True, exist_ok=True)
    if not export_step(model, step_path):
        raise RuntimeError(f"STEP export failed: {step_path}")

    svg_path = preview_svg_path(root)
    svg_path.parent.mkdir(parents=True, exist_ok=True)
    visible, hidden = model.project_to_viewport(PREVIEW_CAMERA, PREVIEW_UP)
    svg = ExportSVG(margin=10)
    svg.add_layer("hidden", line_color=(0.63, 0.63, 0.63), line_type=LineType.ISO_DOT)
    svg.add_layer("visible", line_weight=0.4)
    svg.add_shape(hidden, layer="hidden")
    svg.add_shape(visible, layer="visible")
    svg.write(svg_path)

    return [step_path, svg_path]
