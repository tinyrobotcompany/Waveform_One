"""Clips that hold the screen and LED panel into their pockets.

The LED panel has four clips: two above and two below, CLIP_INSET in from its
left and right edges. The screen has two below and three narrower ones above,
in the gaps between the P4 board's GPIO sockets and microphones on its top
edge. A clip is a flat bar screwed into a heat-set insert in a boss on the
back of the front plate; its free end presses on the component's back edge.
"""

from dataclasses import dataclass

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.components import lcd
from waveform_one.enclosure import FRONT_FACE_DOWN
from waveform_one.geometry import CUT_OVERSHOOT, PrintedPart, box_between, y_cylinder


@dataclass(frozen=True)
class ClipSite:
    name: str
    x: float
    boss_z: float
    edge_z: float  # the component edge this clip reaches over
    rear_y: float  # component's back face; boss top and clip underside
    width: float


def _band_centres() -> tuple[float, float]:
    """Z centres of the bands below and above the component zone."""
    (_, _, z_lo), (_, _, z_hi) = layout.interior_bounds()
    half = layout.component_zone_height() / 2
    centre = layout.zone_centre_z()
    return (z_lo + centre - half) / 2, (z_hi + centre + half) / 2


def clip_sites() -> list[ClipSite]:
    bottom_z, top_z = _band_centres()
    sx, _, sz = layout.screen_origin()
    lx, _, lz = layout.led_origin()
    components = (
        ("screen", sx, sz, p.SCREEN_W.v, p.SCREEN_H.v, layout.screen_rear_y()),
        ("led", lx, lz, p.LED_W.v, p.LED_H.v, layout.led_rear_y()),
    )
    sites = []
    width = p.CLIP_WIDTH.v
    for name, x, z, w, h, rear_y in components:
        for side, cx in (("left", x + p.CLIP_INSET.v), ("right", x + w - p.CLIP_INSET.v)):
            if name == "led":
                sites.append(ClipSite(f"clip_{name}_{side}_top", cx, top_z, z + h, rear_y, width))
            sites.append(ClipSite(f"clip_{name}_{side}_bottom", cx, bottom_z, z, rear_y, width))
    _, _, sz = layout.screen_origin()
    for index, along in enumerate(p.SCREEN_TOP_CLIP_X, start=1):
        sites.append(ClipSite(f"clip_screen_top_{index}", lcd.edge_x(along), top_z, sz + p.SCREEN_H.v,
                              layout.screen_rear_y(), p.SCREEN_TOP_CLIP_WIDTH.v))
    return sites


def build_bosses():
    """Bosses to add to the front plate, and insert holes to cut from them."""
    bosses, holes = [], []
    for site in clip_sites():
        bosses.append(y_cylinder(site.x, p.FRONT_THICKNESS.v, site.rear_y, site.boss_z, p.BOSS_DIAMETER.v))
        holes.append(
            y_cylinder(
                site.x, site.rear_y - p.M3_INSERT_DEPTH.v, site.rear_y + CUT_OVERSHOOT, site.boss_z, p.M3_INSERT_HOLE.v
            )
        )
    return bosses, holes


def build_clips() -> list[PrintedPart]:
    beyond = p.BOSS_DIAMETER.v / 2  # clip extends past the screw by the boss radius
    parts = []
    for site in clip_sites():
        half_w = site.width / 2
        reach = site.edge_z + (p.CLIP_OVERLAP.v if site.boss_z < site.edge_z else -p.CLIP_OVERLAP.v)
        outer = site.boss_z + (-beyond if site.boss_z < site.edge_z else beyond)
        z0, z1 = sorted((reach, outer))
        y0, y1 = site.rear_y, site.rear_y + p.CLIP_THICKNESS.v
        clip = box_between((site.x - half_w, y0, z0), (site.x + half_w, y1, z1))
        clip -= y_cylinder(site.x, y0 - CUT_OVERSHOOT, y1 + CUT_OVERSHOOT, site.boss_z, p.M3_CLEARANCE_HOLE.v)
        parts.append(PrintedPart(site.name, clip, FRONT_FACE_DOWN, "Flat; either face down."))
    return parts

