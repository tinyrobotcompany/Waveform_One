"""Write generated geometry to mechanical/exports/.

Generated files are never written next to source code. Filenames are stable so
a rebuild overwrites the previous output instead of accumulating copies.
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from build123d import Compound, Mesher, Shape, export_step, export_stl
from mpl_toolkits.mplot3d.art3d import Line3DCollection, Poly3DCollection

from waveform_one import parameters as p
from waveform_one.geometry import PrintedPart

EXPORT_ROOT = Path(__file__).resolve().parents[2] / "exports"
REVISION = "m3"
ENVELOPE_LABEL = "Envelope"
MESH_TOLERANCE = 0.01  # mm; well below the 0.4 mm nozzle

# (title, elevation, azimuth) in matplotlib's camera terms; front is -Y, left is -X.
VIEWS = (
    ("Front-left", 22, -125),
    ("Front", 0, -90),
    ("Right side", 0, 0),
    ("Top", 90, -90),
)
LIGHT = np.array([-0.4, -0.7, 0.6]) / np.linalg.norm([-0.4, -0.7, 0.6])


def layout_step_path(root: Path) -> Path:
    return root / "step" / f"waveform_one_{REVISION}_layout.step"


def preview_png_path(root: Path) -> Path:
    return root / "previews" / f"waveform_one_{REVISION}_views.png"


def _leaves(shape: Shape) -> list[Shape]:
    children = list(getattr(shape, "children", ()))
    if not children:
        return [shape]
    return [leaf for child in children for leaf in _leaves(child)]


def _shaded_triangles(leaf: Shape) -> tuple[list[np.ndarray], list[tuple]]:
    vertices, triangles = leaf.tessellate(0.5)
    points = np.array([tuple(v) for v in vertices])
    rgba = tuple(leaf.color) if leaf.color else (0.6, 0.6, 0.6, 1.0)
    polygons, colours = [], []
    for tri in triangles:
        polygon = points[list(tri)]
        normal = np.cross(polygon[1] - polygon[0], polygon[2] - polygon[0])
        length = np.linalg.norm(normal)
        shade = 0.55 + 0.45 * abs(normal @ LIGHT / length) if length else 1.0
        polygons.append(polygon)
        colours.append((*(np.array(rgba[:3]) * shade), rgba[3]))
    return polygons, colours


def render_views(model: Compound, path: Path) -> None:
    polygons, colours, edges = [], [], []
    for leaf in _leaves(model):
        if leaf.label == ENVELOPE_LABEL:
            edges += [[tuple(e.start_point()), tuple(e.end_point())] for e in leaf.edges()]
            continue
        leaf_polygons, leaf_colours = _shaded_triangles(leaf)
        polygons += leaf_polygons
        colours += leaf_colours

    size = model.bounding_box().size
    fig = plt.figure(figsize=(14, 11))
    for index, (title, elev, azim) in enumerate(VIEWS, start=1):
        ax = fig.add_subplot(2, 2, index, projection="3d")
        ax.add_collection3d(Poly3DCollection(polygons, facecolors=colours, edgecolors="none"))
        ax.add_collection3d(Line3DCollection(edges, colors="0.35", linewidths=0.8))
        box = model.bounding_box()
        ax.set_xlim(box.min.X, box.max.X)
        ax.set_ylim(box.min.Y, box.max.Y)
        ax.set_zlim(box.min.Z, box.max.Z)
        ax.set_box_aspect((size.X, size.Y, size.Z))
        ax.set_proj_type("ortho")
        ax.view_init(elev=elev, azim=azim)
        ax.set_title(title)
        ax.set_xlabel("X")
        ax.set_ylabel("Y")
        ax.set_zlabel("Z")
    fig.suptitle(f"Waveform One {REVISION.upper()} layout — orange = keep-out space")
    fig.tight_layout()
    fig.savefig(path, dpi=110)
    plt.close(fig)


def export_model(model: Compound, root: Path) -> list[Path]:
    step_path = layout_step_path(root)
    step_path.parent.mkdir(parents=True, exist_ok=True)
    if not export_step(model, step_path):
        raise RuntimeError(f"STEP export failed: {step_path}")

    png_path = preview_png_path(root)
    png_path.parent.mkdir(parents=True, exist_ok=True)
    render_views(model, png_path)

    return [step_path, png_path]


def printed_part_stem(part: PrintedPart) -> str:
    return f"{part.name}_rev{p.PRINT_REVISION}_{p.PRINT_MATERIAL}"


def export_printed_parts(parts: list[PrintedPart], root: Path) -> list[Path]:
    """3MF and STL per part, already in print orientation and sitting on the bed."""
    written = []
    for part in parts:
        on_bed = part.on_bed()
        stem = printed_part_stem(part)
        mf_path = root / "3mf" / f"{stem}.3mf"
        stl_path = root / "stl" / f"{stem}.stl"
        for path in (mf_path, stl_path):
            path.parent.mkdir(parents=True, exist_ok=True)
        mesher = Mesher()
        mesher.add_shape(on_bed, linear_deflection=MESH_TOLERANCE, part_number=part.name)
        mesher.write(mf_path)
        if not export_stl(on_bed, stl_path, tolerance=MESH_TOLERANCE):
            raise RuntimeError(f"STL export failed: {stl_path}")
        written += [mf_path, stl_path]
    return written
