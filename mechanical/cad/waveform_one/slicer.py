"""ElegooSlicer projects: print jobs that open with their colours already set.

ElegooSlicer (an OrcaSlicer fork) keeps filament colours only when a 3MF is
one of its own projects: anything else loads as bare geometry with colours
it picks itself. A project carries every printer, process and filament
setting, so those come from a project the owner saved once in ElegooSlicer
(the template, mechanical/slicer/ElegooSlicer/template.json). Each print job
takes the template's settings, the colours from colours.toml, and purge
volumes worked out for those colours as ElegooSlicer does.

To refresh the template (another printer, process or filament preset): save
any project from ElegooSlicer with those choices and four filament slots, then

    uv run python -m waveform_one.slicer path/to/saved.3mf
"""

import json
import math
import re
import sys
import uuid
import zipfile
from pathlib import Path
from xml.sax.saxutils import quoteattr

import numpy as np

from waveform_one import colours

TEMPLATE_FILE = Path(__file__).resolve().parents[2] / "slicer" / "ElegooSlicer" / "template.json"
# Purge data shipped with ElegooSlicer; FLUSH_DATA names the file per nozzle_flush_dataset.
RESOURCES = Path("/Applications/ElegooSlicer.app/Contents/Resources")
FLUSH_DATA = {0: "flush_data_standard.txt", 1: "flush_data_dual_standard.txt", 2: "flush_data_dual_highflow.txt"}
CORE = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
PRODUCTION = "http://schemas.microsoft.com/3dmanufacturing/production/2015/06"
BAMBU = "http://schemas.bambulab.com/package/2021"
UUID_NAMESPACE = uuid.UUID("6d3c1f0e-2b7a-4c55-9e0d-8a41f5b2c6d1")


# --- The template ---------------------------------------------------------------------------


def import_template(saved_project: Path, target: Path = TEMPLATE_FILE) -> Path:
    """Keeps a saved ElegooSlicer project's settings and generator tags as the template."""
    with zipfile.ZipFile(saved_project) as package:
        settings = json.loads(package.read("Metadata/project_settings.config"))
        model = package.read("3D/3dmodel.model").decode()
    tags = dict(re.findall(r'<metadata name="(Application|OrcaSlicer)">([^<]*)</metadata>', model))
    if not tags.get("Application", "").startswith("ElegooSlicer-"):
        raise ValueError(f"{saved_project} was not saved by ElegooSlicer")
    if len(settings["filament_colour"]) != len(colours.SLOTS):
        raise ValueError(f"The template needs {len(colours.SLOTS)} filament slots, not {len(settings['filament_colour'])}")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps({
        "_comment": "Settings from a project saved in ElegooSlicer; see waveform_one/slicer.py. "
                    "Colours and purge volumes are replaced from colours.toml when exporting.",
        "application": tags["Application"],
        "orcaslicer": tags.get("OrcaSlicer", ""),
        "project_settings": settings,
    }, indent=1, sort_keys=False) + "\n")
    return target


def load_template(path: Path = TEMPLATE_FILE) -> dict:
    return json.loads(path.read_text())


def project_settings(template: dict, filament_colours: tuple[str, ...], resources: Path = RESOURCES) -> dict:
    """The template's settings with these filament colours and their purge volumes."""
    settings = dict(template["project_settings"])
    if len(filament_colours) != len(settings["filament_colour"]):
        raise ValueError(f"{len(filament_colours)} colours for {len(settings['filament_colour'])} template filament slots")
    settings["filament_colour"] = list(filament_colours)
    settings["filament_multi_colour"] = list(filament_colours)
    settings["flush_volumes_matrix"] = [str(v) for v in flush_matrix(settings, filament_colours, resources)]
    return settings


# --- Purge volumes, as ElegooSlicer 1.5 works them out --------------------------------------
# Ported from ElegooSlicer's FlushVolCalc.cpp, FlushVolPredictor.cpp and Plater.cpp
# (auto_calc_flushing_volumes, get_min_flush_volumes). Its colour formula runs in
# 32-bit floats; so does this, or results differ by 1 mm³.

F = np.float32


def flush_matrix(settings: dict, filament_colours: tuple[str, ...], resources: Path = RESOURCES) -> list[int]:
    """Purge volume (mm³) from each filament to each other one, row by row."""
    printer = settings["printer_settings_id"]
    if _has_printer_rule(printer, resources):
        raise NotImplementedError(f"ElegooSlicer has its own purge table for {printer}; this port does not read it")
    dataset = int(settings["nozzle_flush_dataset"][0])
    data = _load_flush_data(resources / "flush" / FLUSH_DATA[dataset])
    minimum = _min_flush_volume(settings)
    rgb = [colours.rgb(c) for c in filament_colours]
    return [0 if i == j else _flush_volume(a, b, minimum, dataset, data) for i, a in enumerate(rgb) for j, b in enumerate(rgb)]


def _min_flush_volume(settings: dict) -> int:
    if int(settings.get("enable_long_retraction_when_cut", 0)) and settings.get("long_retractions_when_cut", ["0"])[0] == "1":
        raise NotImplementedError("Long retraction when cutting is on; this port assumes it is off")
    return int(float(settings["nozzle_volume"][0]))


def _has_printer_rule(printer: str, resources: Path) -> bool:
    for path in (resources / "profiles").glob("*/flush/flush_volumes.json"):
        for rule in json.loads(path.read_text()).get("rules", []):
            if printer in rule.get("printer_name", []):
                return True
    return False


def _load_flush_data(path: Path):
    """Measured purge volumes between ElegooSlicer's reference colours; None if not installed."""
    if not path.exists():
        return None
    lines = path.read_text().splitlines()
    reference = [colours.rgb(h) for h in lines[1].split()]
    table = {}
    for line in lines[3:]:
        if line.split():
            src, dst, volume = line.split()
            table[(colours.rgb(src), colours.rgb(dst))] = float(volume)
    return reference, table


def _flush_volume(src, dst, minimum: int, dataset: int, data) -> int:
    if dataset != 0 and (measured := _measured(src, dst, data)) is not None:
        return int(measured)
    volume = F(_flush_volume_rgb(src, dst, dataset, data))
    dark_from = sum(F(c) * w for c, w in zip(src, (0.3, 0.59, 0.11))) > 180 / 255
    light_to = sum(F(c) * w for c, w in zip(dst, (0.3, 0.59, 0.11))) < 75 / 255
    if dataset != 0 and dark_from and light_to:  # as in ElegooSlicer, on 0-255 values
        volume = F(volume * F(1.3))
    return int(F(volume + F(minimum)))


def _measured(src, dst, data):
    if data is None:
        return None
    reference, table = data
    similar = lambda colour: next((r for r in reference if _delta_e(_lab(r), _lab(colour)) <= 5.0), None)
    return table.get((similar(src), similar(dst)))


def _flush_volume_rgb(src, dst, dataset: int, data) -> int:
    if dataset == 0 and (measured := _measured(src, dst, data)) is not None:
        return int(F(measured))
    s = [F(v) / F(255) for v in src]
    d = [F(v) / F(255) for v in dst]
    h1, s1, v1 = _hsv(*s)
    h2, s2, v2 = _hsv(*d)
    r1, r2 = _rad(h1), _rad(h2)
    dx = F(np.cos(r1) * s1 * v1) - F(F(np.cos(np.float64(r2))) * s2 * v2)
    dy = F(np.sin(r1) * s1 * v1) - F(F(np.sin(np.float64(r2))) * s2 * v2)
    hs = min(F(1.2), F(np.sqrt(F(dx * dx + dy * dy))))
    luminance = lambda c: F(np.float64(c[0]) * 0.3 + np.float64(c[1]) * 0.59 + np.float64(c[2]) * 0.11)
    from_lumi, to_lumi = luminance(s), luminance(d)
    if to_lumi >= from_lumi:
        lumi = F(F(np.power(F(to_lumi - from_lumi), F(0.7))) * F(560))
    else:
        lumi = F(F(from_lumi - to_lumi) * F(80))
        hs = min(F(0.67 * np.float64(v2) + 0.33 * np.float64(v1)), hs)
    hue = F(F(230) * hs)
    volume = F(np.sqrt(F(hue * hue + lumi * lumi - F(2) * hue * lumi * F(np.cos(_rad(120))))))
    return int(max(volume, F(60)))


def _rad(degrees) -> np.float32:
    return F(F(degrees) / F(180) * F(np.pi))


def _hsv(r, g, b):
    high, low = max(r, g, b), min(r, g, b)
    delta = F(high - low)
    if abs(delta) < 0.001:
        hue = F(0)
    elif high == r:
        hue = F(60) * F(np.fmod(F((g - b) / delta), F(6)))
    elif high == g:
        hue = F(60) * F((b - r) / delta + F(2))
    else:
        hue = F(60) * F((r - g) / delta + F(4))
    saturation = F(0) if abs(high) < 0.001 else F(delta / high)
    return hue, saturation, F(high)


def _lab(rgb):
    gamma = lambda x: ((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92
    r, g, b = (gamma(v / 255) * 100 for v in rgb)
    x = 0.412453 * r + 0.357580 * g + 0.180423 * b
    y = 0.212671 * r + 0.715160 * g + 0.072169 * b
    z = 0.019334 * r + 0.119193 * g + 0.950227 * b
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 0.137931
    xn, yn, zn = f(x / 95.0489), f(y / 100), f(z / 108.8840)
    return 116 * yn - 16, 500 * (xn - yn), 200 * (yn - zn)


def _delta_e(lab1, lab2) -> float:
    """CIEDE2000 colour difference."""
    p25 = 25**7
    c1, c2 = math.hypot(lab1[1], lab1[2]), math.hypot(lab2[1], lab2[2])
    c_mean = (c1 + c2) / 2
    g = 0.5 * (1 - math.sqrt(c_mean**7 / (c_mean**7 + p25)))
    a1, a2, b1, b2 = (1 + g) * lab1[1], (1 + g) * lab2[1], lab1[2], lab2[2]
    pc1, pc2 = math.hypot(a1, b1), math.hypot(a2, b2)
    h1 = 0 if a1 == 0 and b1 == 0 else math.atan2(b1, a1) % (2 * math.pi)
    h2 = 0 if a2 == 0 and b2 == 0 else math.atan2(b2, a2) % (2 * math.pi)
    d_l, d_c = lab2[0] - lab1[0], pc2 - pc1
    if pc1 * pc2 == 0:
        d_h = 0
    else:
        d_h = h2 - h1
        if d_h < -math.pi:
            d_h += 2 * math.pi
        elif d_h > math.pi:
            d_h -= 2 * math.pi
        d_h = 2 * math.sqrt(pc1 * pc2) * math.sin(d_h / 2)
    l_mean, pc_mean, h_sum = (lab1[0] + lab2[0]) / 2, (pc1 + pc2) / 2, h1 + h2
    if pc1 * pc2 == 0:
        h_mean = h_sum
    elif abs(h1 - h2) <= math.pi:
        h_mean = h_sum / 2
    else:
        h_mean = (h_sum + 2 * math.pi) / 2 if h_sum < 2 * math.pi else (h_sum - 2 * math.pi) / 2
    r = math.radians
    t = (1 - 0.17 * math.cos(h_mean - r(30)) + 0.24 * math.cos(2 * h_mean)
         + 0.32 * math.cos(3 * h_mean + r(6)) - 0.2 * math.cos(4 * h_mean - r(63)))
    d_theta = r(30) * math.exp(-(((h_mean - r(275)) / r(25)) ** 2))
    r_c = 2 * math.sqrt(pc_mean**7 / (pc_mean**7 + p25))
    s_l = 1 + 0.015 * (l_mean - 50) ** 2 / math.sqrt(20 + (l_mean - 50) ** 2)
    s_c, s_h = 1 + 0.045 * pc_mean, 1 + 0.015 * pc_mean * t
    r_t = -math.sin(2 * d_theta) * r_c
    return math.sqrt((d_l / s_l) ** 2 + (d_c / s_c) ** 2 + (d_h / s_h) ** 2 + r_t * (d_c / s_c) * (d_h / s_h))


# --- The project file -----------------------------------------------------------------------


def _uuid(*names) -> str:
    return str(uuid.uuid5(UUID_NAMESPACE, "/".join(map(str, names))))


def _matrix(x: float = 0.0, y: float = 0.0) -> str:
    return f"1 0 0 0 1 0 0 0 1 {x:g} {y:g} 0"


def bed_centre(settings: dict) -> tuple[float, float]:
    corners = [tuple(map(float, corner.split("x"))) for corner in settings["printable_area"]]
    xs, ys = zip(*corners)
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2


def write_project(path: Path, objects, settings: dict, template: dict, mesh_xml) -> None:
    """An ElegooSlicer project: one object per printed part, its bands as parts on their filaments.

    `objects` is [(name, [Band])], with the shapes already laid out around the
    bed origin; the project places that origin at the bed centre.
    `mesh_xml(shape)` writes a closed <mesh> element.
    """
    cx, cy = bed_centre(settings)
    header = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<model unit="millimeter" xml:lang="en-US" xmlns="{CORE}" xmlns:BambuStudio="{BAMBU}" '
        f'xmlns:p="{PRODUCTION}" requiredextensions="p">'
    )
    files, components, items, settings_xml, instances, assemble = {}, [], [], [], [], []
    next_id = 1
    for index, (name, bands) in enumerate(objects, start=1):
        object_path = f"/3D/Objects/{re.sub(r'[^A-Za-z0-9_]', '_', name)}_{index}.model"
        meshes, parts, part_ids = [], [], []
        for band in bands:
            meshes.append(f'<object id="{next_id}" p:UUID="{_uuid(name, band.name)}" type="model">{mesh_xml(band.body)}</object>')
            parts.append(f'<component p:path="{object_path}" objectid="{next_id}" p:UUID="{_uuid(name, band.name, "component")}" '
                         f'transform="{_matrix()}"/>')
            part_ids.append((next_id, band))
            next_id += 1
        files[object_path.lstrip("/")] = (f'{header}<metadata name="BambuStudio:3mfVersion">1</metadata>'
                                          f'<resources>{"".join(meshes)}</resources><build/></model>')
        object_id = next_id
        next_id += 1
        components.append(f'<object id="{object_id}" p:UUID="{_uuid(name)}" type="model"><components>{"".join(parts)}</components></object>')
        items.append(f'<item objectid="{object_id}" p:UUID="{_uuid(name, "item")}" transform="{_matrix(cx, cy)}" printable="1"/>')
        part_settings = "".join(
            f'<part id="{part_id}" subtype="normal_part"><metadata key="name" value={quoteattr(band.name)}/>'
            f'<metadata key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>'
            f'<metadata key="extruder" value="{band.filament}"/></part>'
            for part_id, band in part_ids
        )
        settings_xml.append(f'<object id="{object_id}"><metadata key="name" value={quoteattr(name)}/>'
                            f'<metadata key="extruder" value="1"/>{part_settings}</object>')
        instances.append(f'<model_instance><metadata key="object_id" value="{object_id}"/>'
                         f'<metadata key="instance_id" value="0"/><metadata key="identify_id" value="{index}"/></model_instance>')
        assemble.append(f'<assemble_item object_id="{object_id}" instance_id="0" transform="{_matrix(cx, cy)}" offset="0 0 0"/>')

    model = (
        f"{header}"
        f'<metadata name="Application">{template["application"]}</metadata>'
        + (f'<metadata name="OrcaSlicer">{template["orcaslicer"]}</metadata>' if template["orcaslicer"] else "")
        + '<metadata name="BambuStudio:3mfVersion">1</metadata>'
        f'<metadata name="Title">{path.stem}</metadata>'
        f'<resources>{"".join(components)}</resources>'
        f'<build p:UUID="{_uuid(path.stem, "build")}">{"".join(items)}</build></model>'
    )
    model_settings = (
        '<?xml version="1.0" encoding="UTF-8"?>\n<config>'
        + "".join(settings_xml)
        + '<plate><metadata key="plater_id" value="1"/><metadata key="plater_name" value=""/>'
        '<metadata key="locked" value="false"/>' + "".join(instances) + "</plate>"
        + "<assemble>" + "".join(assemble) + "</assemble></config>"
    )
    object_rels = "".join(
        f'<Relationship Target="/{target}" Id="rel-{n}" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
        for n, target in enumerate(files, start=1)
    )
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as package:
        package.writestr(
            "[Content_Types].xml",
            '<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
            '<Default Extension="png" ContentType="image/png"/><Default Extension="gcode" ContentType="text/x.gcode"/></Types>',
        )
        package.writestr(
            "_rels/.rels",
            '<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
            "</Relationships>",
        )
        package.writestr("3D/3dmodel.model", model)
        package.writestr(
            "3D/_rels/3dmodel.model.rels",
            f'<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">{object_rels}</Relationships>',
        )
        for name, content in files.items():
            package.writestr(name, content)
        package.writestr("Metadata/project_settings.config", json.dumps(settings, indent=4))
        package.writestr("Metadata/model_settings.config", model_settings)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: python -m waveform_one.slicer path/to/project-saved-by-ElegooSlicer.3mf")
    print(f"Wrote {import_template(Path(sys.argv[1]))}")
