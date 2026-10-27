"""Write generated geometry to mechanical/exports/.

Generated files are never written next to source code. Filenames are stable so
a rebuild overwrites the previous output instead of accumulating copies.
"""

import zipfile
from collections import Counter
from pathlib import Path
from xml.sax.saxutils import quoteattr

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from build123d import Compound, Location, Part, Pos, Shape, export_step, export_stl
from OCP.BOPAlgo import BOPAlgo_Builder
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from mpl_toolkits.mplot3d.art3d import Line3DCollection, Poly3DCollection

from waveform_one import colours, slicer
from waveform_one import parameters as p
from waveform_one.geometry import Band, PrintedPart

EXPORT_ROOT = Path(__file__).resolve().parents[2] / "exports"
REVISION = "revB"
ENVELOPE_LABEL = "Envelope"
MESH_TOLERANCE = 0.01  # mm; well below the 0.4 mm nozzle
ANGULAR_TOLERANCE = 0.1  # radians
WELD_DECIMALS = 5  # points within 0.00001 mm are the same point

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
    fig.suptitle(f"Waveform One {REVISION.upper()} layout — orange = keep-out space; printed parts in the colours.toml colours")
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


def _mesh_xml(shape: Shape) -> str:
    """A closed (manifold) triangle mesh.

    The tessellator meshes each face on its own, repeating the shared edge
    points; they are welded here so neighbouring faces share their edges, as
    slicers require.
    """
    vertices, triangles = shape.tessellate(MESH_TOLERANCE, ANGULAR_TOLERANCE)
    index_of, points, remap = {}, [], []
    for v in vertices:
        key = (round(v.X, WELD_DECIMALS), round(v.Y, WELD_DECIMALS), round(v.Z, WELD_DECIMALS))
        if key not in index_of:
            index_of[key] = len(points)
            points.append(key)
        remap.append(index_of[key])
    faces = []
    for a, b, c in triangles:
        a, b, c = remap[a], remap[b], remap[c]
        if a != b and b != c and a != c:  # welding can collapse a sliver to a line
            faces.append(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>')
    vertex_xml = "".join(f'<vertex x="{x:.5f}" y="{y:.5f}" z="{z:.5f}"/>' for x, y, z in points)
    return f"<mesh><vertices>{vertex_xml}</vertices><triangles>{''.join(faces)}</triangles></mesh>"


def write_3mf(path: Path, objects: list[tuple[str, list[Band]]]) -> None:
    """A 3MF with one object per printed part, each made of its colour bands.

    The bands are components of their object, so slicers load them as parts of
    one object that cannot drift apart. Each part is named after its band;
    Metadata/model_settings.config also records the band's filament for
    slicers that read it (Bambu Studio, OrcaSlicer and its forks). The shapes
    are already in bed position.
    """
    resources, items, settings, next_id = [], [], [], 1
    for name, bands in objects:
        parts = []
        for band in bands:
            resources.append(f"<object id=\"{next_id}\" type=\"model\" name={quoteattr(band.name)}>{_mesh_xml(band.body)}</object>")
            parts.append((next_id, band))
            next_id += 1
        components = "".join(f'<component objectid="{part_id}"/>' for part_id, _ in parts)
        resources.append(f"<object id=\"{next_id}\" type=\"model\" name={quoteattr(name)}><components>{components}</components></object>")
        items.append(f'<item objectid="{next_id}"/>')
        part_settings = "".join(
            f'<part id="{part_id}" subtype="normal_part"><metadata key="name" value={quoteattr(band.name)}/>'
            f'<metadata key="extruder" value="{band.filament}"/></part>'
            for part_id, band in parts
        )
        settings.append(
            f'<object id="{next_id}"><metadata key="name" value={quoteattr(name)}/>'
            f'<metadata key="extruder" value="1"/>{part_settings}</object>'
        )
        next_id += 1
    model = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
        f'<metadata name="Application">Waveform One CAD (build123d)</metadata>'
        f'<resources>{"".join(resources)}</resources><build>{"".join(items)}</build></model>'
    )
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as package:
        package.writestr(
            "[Content_Types].xml",
            '<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
            '<Default Extension="config" ContentType="text/xml"/></Types>',
        )
        package.writestr(
            "_rels/.rels",
            '<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
            "</Relationships>",
        )
        package.writestr("3D/3dmodel.model", model)
        package.writestr(
            "Metadata/model_settings.config",
            f'<?xml version="1.0" encoding="UTF-8"?>\n<config>{"".join(settings)}</config>',
        )


def paint_code(filament: int) -> str:
    """A whole triangle painted with `filament`, as Bambu Studio, OrcaSlicer and PrusaSlicer store it.

    Their TriangleSelector serialisation: states 1-2 are one hex digit
    (state << 2); higher states are the marker C plus (state - 3), written
    last digit first.
    """
    if not 1 <= filament <= 18:
        raise ValueError(f"filament {filament} cannot be painted")
    return f"{filament << 2:X}" if filament < 3 else f"{filament - 3:X}C"


def painted_mesh(bands: list[Band]) -> tuple[list[tuple[float, float, float]], list[tuple[int, int, int, int]]]:
    """The bands glued into one closed mesh, each triangle tagged with its band's filament.

    A plain fuse can merge faces of neighbouring bands into one face that
    crosses a colour boundary. The general fuse (BOPAlgo_Builder) instead
    keeps every band's faces apart and shares the faces where bands touch;
    those shared faces are inside the part and left out, and the remaining
    faces are meshed together so neighbouring bands share their edge points.
    """
    if len(bands) == 1:  # one colour: nothing to glue
        vertices, faces = bands[0].body.tessellate(MESH_TOLERANCE, ANGULAR_TOLERANCE)
        return _welded(vertices, faces, bands[0].filament)
    builder = BOPAlgo_Builder()
    for band in bands:
        for solid in band.body.solids():
            builder.AddArgument(solid.wrapped)
    builder.Perform()
    if builder.HasErrors():
        raise RuntimeError("Could not glue the colour bands together")
    pieces = Compound(builder.Shape()).solids()
    filament_of = {}
    for band in bands:
        for solid in band.body.solids():
            images = list(builder.Modified(solid.wrapped)) or [solid.wrapped]
            for image in images:
                for piece in pieces:
                    if piece.wrapped.IsSame(image):
                        filament_of[piece] = band.filament
    if len(filament_of) != len(pieces):
        raise RuntimeError("A glued piece belongs to no colour band")
    BRepMesh_IncrementalMesh(builder.Shape(), MESH_TOLERANCE, False, ANGULAR_TOLERANCE, True)
    faces = Counter(face for piece in pieces for face in piece.faces())
    index_of, points, triangles = {}, [], []
    for piece in pieces:
        for face in piece.faces():
            if faces[face] > 1:  # where two bands touch: inside the part
                continue
            vertices, face_triangles = face.tessellate(MESH_TOLERANCE, ANGULAR_TOLERANCE)
            remap = []
            for v in vertices:
                key = (round(v.X, WELD_DECIMALS), round(v.Y, WELD_DECIMALS), round(v.Z, WELD_DECIMALS))
                if key not in index_of:
                    index_of[key] = len(points)
                    points.append(key)
                remap.append(index_of[key])
            for a, b, c in face_triangles:
                a, b, c = remap[a], remap[b], remap[c]
                if a != b and b != c and a != c:
                    triangles.append((a, b, c, filament_of[piece]))
    return points, triangles


def _welded(vertices, faces, filament: int):
    index_of, points, remap = {}, [], []
    for v in vertices:
        key = (round(v.X, WELD_DECIMALS), round(v.Y, WELD_DECIMALS), round(v.Z, WELD_DECIMALS))
        if key not in index_of:
            index_of[key] = len(points)
            points.append(key)
        remap.append(index_of[key])
    triangles = [(remap[a], remap[b], remap[c], filament) for a, b, c in faces
                 if len({remap[a], remap[b], remap[c]}) == 3]
    return points, triangles


def write_painted_3mf(path: Path, objects: list[tuple[str, list[Band]]]) -> None:
    """One closed mesh per printed part with its colours painted on, for print services.

    Online print services (PrintYourModel, Zbotic and others) read colours
    painted on the triangles, as Bambu Studio and OrcaSlicer save them, not
    the colour parts that the slicer projects use. Each triangle carries its
    filament as paint_color (Bambu Studio, OrcaSlicer) and
    slic3rpe:mmu_segmentation (PrusaSlicer).

    Slots with the same colours.toml colour share one filament, and every
    triangle is painted, the body's too: PrintYourModel (order.js) counts
    the distinct characters of the paint_color values, so an unpainted body
    is not counted and each two-character code ("0C") counts twice. With
    black and white, the codes "4" and "8" count as the two colours they are.
    The shapes are already laid out on the bed.
    """
    slot_colours = colours.load()
    distinct, shop_filament = colours.filaments(slot_colours)

    def paint(filament: int) -> str:
        code = paint_code(shop_filament[filament])
        return f' paint_color="{code}" slic3rpe:mmu_segmentation="{code}"'

    resources, items, settings, used = [], [], [], set()
    for object_id, (name, bands) in enumerate(objects, start=1):
        points, triangles = painted_mesh(bands)
        used |= {shop_filament[f] for *_, f in triangles}
        vertex_xml = "".join(f'<vertex x="{x:.5f}" y="{y:.5f}" z="{z:.5f}"/>' for x, y, z in points)
        triangle_xml = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"{paint(f)}/>' for a, b, c, f in triangles)
        resources.append(f'<object id="{object_id}" type="model" name={quoteattr(name)}><mesh><vertices>{vertex_xml}</vertices>'
                         f'<triangles>{triangle_xml}</triangles></mesh></object>')
        items.append(f'<item objectid="{object_id}"/>')
        settings.append(f'<object id="{object_id}"><metadata key="name" value={quoteattr(name)}/>'
                        '<metadata key="extruder" value="1"/></object>')
    model = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" '
        'xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" xmlns:slic3rpe="http://schemas.slic3r.org/3mf/2017/06">'
        '<metadata name="Application">Waveform One CAD (build123d)</metadata>'
        '<metadata name="slic3rpe:MmPaintingVersion">1</metadata>'
        f'<metadata name="Description">Filaments: {"; ".join(f"{f} = {distinct[f - 1]}" for f in sorted(used))}</metadata>'
        f'<resources>{"".join(resources)}</resources><build>{"".join(items)}</build></model>'
    )
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as package:
        package.writestr(
            "[Content_Types].xml",
            '<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
            '<Default Extension="config" ContentType="text/xml"/></Types>',
        )
        package.writestr(
            "_rels/.rels",
            '<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
            "</Relationships>",
        )
        package.writestr("3D/3dmodel.model", model)
        package.writestr(
            "Metadata/model_settings.config",
            f'<?xml version="1.0" encoding="UTF-8"?>\n<config>{"".join(settings)}</config>',
        )


def export_printed_parts(parts: list[PrintedPart], root: Path) -> list[Path]:
    """3MF and STL per part, already in print orientation and sitting on the bed."""
    written = []
    for part in parts:
        stem = printed_part_stem(part)
        mf_path = root / "3mf" / f"{stem}.3mf"
        stl_path = root / "stl" / f"{stem}.stl"
        for path in (mf_path, stl_path):
            path.parent.mkdir(parents=True, exist_ok=True)
        write_3mf(mf_path, [(part.name, part.bands_on_bed())])
        if not export_stl(part.on_bed(), stl_path, tolerance=MESH_TOLERANCE):
            raise RuntimeError(f"STL export failed: {stl_path}")
        written += [mf_path, stl_path]
    return written


def plate_locations(parts: list[PrintedPart]) -> list[tuple[PrintedPart, Location]]:
    """Where each part goes on one plate: rows in the order given, centred on the bed origin.

    Each location maps the part's assembly position to its plate position.
    Raises if the layout exceeds MAX_PRINT_X by PLATE_DEPTH.
    """
    gap, max_x = p.PLATE_GAP.v, p.MAX_PRINT_X.v
    rows: list[list[PrintedPart]] = [[]]
    row_width = 0.0
    for part in parts:
        width = part.on_bed().bounding_box().size.X
        if rows[-1] and row_width + gap + width > max_x:
            rows.append([])
            row_width = 0.0
        row_width += (gap if rows[-1] else 0.0) + width
        rows[-1].append(part)

    shifts, y = [], 0.0
    for row in rows:
        x = 0.0
        boxes = [part.on_bed().bounding_box() for part in row]
        for part, box in zip(row, boxes):
            shifts.append((part, Pos(x - box.min.X, y - box.min.Y, 0)))
            x += box.size.X + gap
        y += max(box.size.Y for box in boxes) + gap

    total = Compound(children=[shift * part.on_bed() for part, shift in shifts]).bounding_box()
    if total.size.X > max_x + 1e-3 or total.size.Y > p.PLATE_DEPTH.v + 1e-3:
        names = ", ".join(part.name for part in parts)
        raise RuntimeError(f"Plate {total.size.X:.0f} x {total.size.Y:.0f} mm is too big for: {names}")
    centre = Pos(-total.center().X, -total.center().Y, 0)
    return [(part, centre * shift * part.bed_location()) for part, shift in shifts]


def arrange_plate(parts: list[PrintedPart]) -> list[tuple[PrintedPart, Part]]:
    """Every part laid out on one plate (see plate_locations)."""
    return [(part, location * part.body) for part, location in plate_locations(parts)]


def export_print_job(name: str, parts: list[PrintedPart], root: Path) -> Path:
    """One ElegooSlicer project per print job: every part as its own object, already laid out on the bed.

    Parts with colour bands keep them as named parts, each on the filament
    of its colours.toml colour: one filament per distinct colour, so bands of
    the same colour share one spool (see slicer.py).
    """
    path = root / "print_jobs" / f"{name}_rev{p.PRINT_REVISION}_{p.PRINT_MATERIAL}.3mf"
    path.parent.mkdir(parents=True, exist_ok=True)
    slot_colours = colours.load()
    placed = [(part, location, part.bands or (Band(part.name, 1, part.body),)) for part, location in plate_locations(parts)]
    used = {band.filament for _, _, bands in placed for band in bands}
    _, filament = colours.filaments(slot_colours, used)
    objects = [(part.name, [Band(band.name, filament[band.filament], location * band.body) for band in bands])
               for part, location, bands in placed]
    template = slicer.load_template()
    settings = slicer.project_settings(template, slot_colours, used)
    slicer.write_project(path, objects, settings, template, _mesh_xml)
    return path


def export_print_shop_job(name: str, parts: list[PrintedPart], root: Path) -> Path:
    """The print job for online print services: same name and layout as in print_jobs/, colours painted on."""
    path = root / "print_shop" / f"{name}_rev{p.PRINT_REVISION}_{p.PRINT_MATERIAL}.3mf"
    path.parent.mkdir(parents=True, exist_ok=True)
    objects = []
    for part, location in plate_locations(parts):
        bands = part.bands or (Band(part.name, 1, part.body),)
        objects.append((part.name, [Band(band.name, band.filament, location * band.body) for band in bands]))
    write_painted_3mf(path, objects)
    return path
