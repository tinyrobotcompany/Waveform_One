"""Print jobs as ElegooSlicer projects, with the colours.toml colours."""

import json
import xml.etree.ElementTree as ET
import zipfile

import pytest

from waveform_one import colours, slicer

CORE = {"m": slicer.CORE}


def test_colours_file_gives_one_colour_per_slot():
    loaded = colours.load()
    assert len(loaded) == len(colours.SLOTS)
    assert all(colours.HEX.fullmatch(c) and c == c.upper() for c in loaded)


@pytest.mark.parametrize(
    "text, message",
    [
        ('body = "#000000"\nstripe_1 = "#C12E1F"\nstripe_2 = "#0A2989"\n', "missing ['name']"),
        ('body = "#000000"\nstripe_1 = "red"\nstripe_2 = "#0A2989"\nname = "#FFFFFF"\n', "stripe_1 = 'red'"),
        ('body = "#000000"\nstripe_1 = "#C12E1F"\nstripe_2 = "#0A2989"\nname = "#FFFFFF"\nbase = "#111111"\n', "unknown ['base']"),
    ],
)
def test_colours_file_mistakes_are_explained(tmp_path, text, message):
    path = tmp_path / "colours.toml"
    path.write_text(text)
    with pytest.raises(ValueError, match=message.replace("[", r"\[").replace("]", r"\]")):
        colours.load(path)


@pytest.mark.skipif(not (slicer.RESOURCES / "flush").exists(), reason="ElegooSlicer is not installed")
def test_purge_volumes_match_what_elegooslicer_worked_out():
    """The template stores ElegooSlicer's own purge volumes for the colours it was saved with."""
    settings = slicer.load_template()["project_settings"]
    stored = [int(float(v)) for v in settings["flush_volumes_matrix"]]
    assert slicer.flush_matrix(settings, tuple(c.upper() for c in settings["filament_colour"])) == stored


def test_project_settings_take_the_colours_and_keep_the_template():
    template = slicer.load_template()
    chosen = ("#112233", "#445566", "#778899", "#AABBCC")
    settings = slicer.project_settings(template, chosen)
    assert settings["filament_colour"] == settings["filament_multi_colour"] == list(chosen)
    assert len(settings["flush_volumes_matrix"]) == len(chosen) ** 2
    assert all(settings["flush_volumes_matrix"][i * 5] == "0" for i in range(4))
    unchanged = set(settings) - {"filament_colour", "filament_multi_colour", "flush_volumes_matrix"}
    assert all(settings[k] == template["project_settings"][k] for k in unchanged)
    with pytest.raises(ValueError):
        slicer.project_settings(template, chosen[:3])


def test_slots_of_the_same_colour_share_one_filament():
    """Black body, white stripes and name: two filaments, so two spools, not four."""
    template = slicer.load_template()
    source = template["project_settings"]
    settings = slicer.project_settings(template, ("#000000", "#FFFFFF", "#FFFFFF", "#FFFFFF"))
    assert settings["filament_colour"] == ["#000000", "#FFFFFF"]
    assert colours.filaments(("#000000", "#FFFFFF", "#FFFFFF", "#FFFFFF"))[1] == {1: 1, 2: 2, 3: 2, 4: 2}
    for key in slicer.filament_keys():
        if key in settings:
            assert len(settings[key]) == 2, key
    assert settings["filament_settings_id"] == source["filament_settings_id"][:2]
    assert len(settings["flush_volumes_matrix"]) == 4
    assert len(settings["inherits_group"]) == len(settings["different_settings_to_system"]) == 4
    assert settings["filament_self_index"] == ["1", "2"]
    # Settings that only happen to have four values are left alone.
    assert settings["printable_area"] == source["printable_area"]


def test_template_is_a_four_slot_elegooslicer_project():
    template = slicer.load_template()
    assert template["application"].startswith("ElegooSlicer-")
    assert len(template["project_settings"]["filament_colour"]) == len(colours.SLOTS)


def test_shell_print_job_is_a_project_with_each_band_on_its_coloured_slot(tmp_path):
    from waveform_one.enclosure.shell import build_halves
    from waveform_one.export import export_print_job

    part = next(half for half in build_halves() if half.name == "shell_right")
    path = export_print_job("shell", [part], tmp_path)
    with zipfile.ZipFile(path) as package:
        model = ET.fromstring(package.read("3D/3dmodel.model"))
        config = ET.fromstring(package.read("Metadata/model_settings.config"))
        settings = json.loads(package.read("Metadata/project_settings.config"))
        meshes = {o.get("id"): o for name in package.namelist() if name.startswith("3D/Objects/")
                  for o in ET.fromstring(package.read(name)).find("m:resources", CORE)}

    application = model.find("m:metadata[@name='Application']", CORE).text
    assert application == slicer.load_template()["application"]
    distinct, filament = colours.filaments(colours.load())
    assert settings["filament_colour"] == distinct
    # One object, its bands as parts in band order, each on the filament of its colour.
    (obj,) = config.iter("object")
    named = [(pt.find("metadata[@key='name']").get("value"), pt.find("metadata[@key='extruder']").get("value"))
             for pt in obj.iter("part")]
    assert named == [(band.name, str(filament[band.filament])) for band in part.bands]
    # Every part's mesh is there, and the object sits at the bed centre.
    assert sorted(meshes) == sorted(pt.get("id") for pt in obj.iter("part"))
    (item,) = model.find("m:build", CORE)
    cx, cy = slicer.bed_centre(settings)
    assert item.get("transform").split()[-3:] == [f"{cx:g}", f"{cy:g}", "0"]


def test_paint_codes_match_bambu_and_orca_files():
    from waveform_one.export import paint_code

    assert [paint_code(f) for f in (1, 2, 3, 4, 5)] == ["4", "8", "0C", "1C", "2C"]


def test_print_shop_shell_is_one_closed_mesh_painted_by_band(tmp_path):
    """Print services read painted triangles: one closed mesh, every triangle on its band's filament."""
    from collections import Counter

    from waveform_one.enclosure.shell import build_halves
    from waveform_one.export import export_print_shop_job, paint_code

    part = next(half for half in build_halves() if half.name == "shell_right")
    path = export_print_shop_job("3_shell_right", [part], tmp_path)
    assert path.relative_to(tmp_path).as_posix() == "print_shop/3_shell_right_revB_PETG.3mf"
    with zipfile.ZipFile(path) as package:
        model = ET.fromstring(package.read("3D/3dmodel.model"))
    (obj,) = model.find("m:resources", CORE)
    triangles = list(obj.find("m:mesh/m:triangles", CORE))
    edges = Counter()
    for t in triangles:
        a, b, c = (int(t.get(k)) for k in ("v1", "v2", "v3"))
        edges.update([(a, b), (b, c), (c, a)])
    assert all(n == 1 and edges[(b, a)] == 1 for (a, b), n in edges.items())
    # Every triangle painted; slots of the same colour on one filament.
    slot_colours = colours.load()
    distinct = list(dict.fromkeys(slot_colours))
    expected = {paint_code(distinct.index(slot_colours[b.filament - 1]) + 1) for b in part.bands}
    codes = {t.get("paint_color") for t in triangles}
    assert codes == expected
    # PrintYourModel's order.js counts the distinct characters of all paint_color values.
    used = {slot_colours[b.filament - 1] for b in part.bands}
    if len(used) <= 2:
        assert len(set("".join(codes))) == len(used)
