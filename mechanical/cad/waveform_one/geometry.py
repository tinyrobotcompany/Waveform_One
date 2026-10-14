"""Small geometry helpers shared by component and enclosure modules."""

from dataclasses import dataclass, field

from build123d import Align, Box, Color, Compound, Cylinder, Location, Part, Plane, Polygon, Pos, RectangleRounded, Rot, extrude

Vec3 = tuple[float, float, float]

MIN_ALIGN = (Align.MIN, Align.MIN, Align.MIN)
KEEPOUT_COLOR = Color(1.0, 0.45, 0.1, 0.35)
# How far a cutting tool extends past the faces it cuts, so no zero-thickness skins remain.
CUT_OVERSHOOT = 1.0


def box(min_corner: Vec3, size: Vec3) -> Part:
    """Axis-aligned box given its minimum corner and its X/Y/Z extents."""
    return Pos(*min_corner) * Box(*size, align=MIN_ALIGN)


def box_between(lo: Vec3, hi: Vec3) -> Part:
    return box(lo, (hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))


def front_plane(y: float, centre_z: float, centre_x: float = 0.0) -> Plane:
    """A plane facing the viewer (-Y) with local x = world X and local y = world Z."""
    return Plane(origin=(centre_x, y, centre_z), x_dir=(1, 0, 0), z_dir=(0, -1, 0))


def rounded_prism(width: float, height: float, radius: float, y0: float, length: float, centre_z: float) -> Part:
    """Rounded rectangle in the XZ plane, centred on X=0, extruded rearward from y0."""
    sketch = front_plane(y0, centre_z) * RectangleRounded(width, height, radius)
    return extrude(sketch, amount=length, dir=(0, 1, 0))


def y_cylinder(x: float, y0: float, y1: float, z: float, diameter: float) -> Part:
    """Cylinder whose axis runs along Y from y0 to y1."""
    return Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(diameter / 2, y1 - y0)


def z_cylinder(x: float, y: float, z0: float, z1: float, diameter: float) -> Part:
    return Pos(x, y, (z0 + z1) / 2) * Cylinder(diameter / 2, z1 - z0)


def sloped_pad(x_centre: float, width: float, y0: float, y1: float, wall_z: float, protrusion: float, up: bool) -> Part:
    """A block standing off a floor (up=True) or hanging from a ceiling (up=False).

    Shells print front-face down, so world Y is the print's vertical axis. The
    pad's front face is sloped at 45 degrees (it starts `protrusion` in front of
    y0 at zero height) so it never overhangs during printing. Full height runs
    from y0 to y1. The base sinks CUT_OVERSHOOT into the wall so they fuse.
    """
    sign = 1 if up else -1
    base = wall_z - sign * CUT_OVERSHOOT
    top = wall_z + sign * protrusion
    profile = [(y0 - protrusion - CUT_OVERSHOOT, base), (y0, top), (y1, top), (y1, base)]
    plane = Plane(origin=(x_centre - width / 2, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    return extrude(plane * Polygon(*profile, align=None), amount=width, dir=(1, 0, 0))


@dataclass
class Placeholder:
    """An electronics stand-in: its solid body plus spaces that must stay clear.

    Keep-outs reserve room for connectors, cable plugs and bends. Other
    components' bodies must not enter them; keep-outs may overlap each other.
    """

    name: str
    body: Part
    color: Color
    keepouts: dict[str, Part] = field(default_factory=dict)
    markers: dict[str, Part] = field(default_factory=dict)  # visual references inside the body

    def compound(self) -> Compound:
        self.body.label = f"{self.name}Body"
        self.body.color = self.color
        children = [self.body]
        for name, marker in self.markers.items():
            marker.label = name
            marker.color = Color(0.2, 0.6, 1.0)
            children.append(marker)
        for name, keepout in self.keepouts.items():
            keepout.label = f"KeepOut_{name}"
            keepout.color = KEEPOUT_COLOR
            children.append(keepout)
        return Compound(label=self.name, children=children)


PRINT_BLACK = Color(0.12, 0.12, 0.13)
# Preview only: the real colours are chosen in the slicer (filament 1, 2, 3).
BAND_PREVIEW_COLOURS = (PRINT_BLACK, Color(0.85, 0.12, 0.12), Color(0.15, 0.4, 0.9), Color(0.97, 0.97, 0.97))


@dataclass
class Band:
    """One colour region of a printed part, printed with slicer filament `filament` (1 = the body colour)."""

    name: str
    filament: int
    body: Part


@dataclass
class PrintedPart:
    """A part to be printed.

    `body` is in assembly position (world coordinates). `print_pose` rotates it
    into its recommended print orientation; `on_bed()` then sits it on Z=0,
    centred on the bed origin.
    """

    name: str
    body: Part
    print_pose: Location
    orientation_note: str
    # Mock-up-only accessories (e.g. stands) never exist inside the real
    # enclosure, so they are excluded from internal collision checks.
    mockup_only: bool = False
    # Colour regions that together make up `body`, for multi-filament printing.
    # Empty: the whole part prints in filament 1.
    bands: tuple[Band, ...] = ()

    def bed_location(self) -> Location:
        """Print pose, then sit on Z=0 centred on the bed origin."""
        box = (self.print_pose * self.body).bounding_box()
        return Pos(-box.center().X, -box.center().Y, -box.min.Z) * self.print_pose

    def on_bed(self) -> Part:
        return self.bed_location() * self.body

    def bands_on_bed(self) -> list[Band]:
        """The colour regions in print position (one band, filament 1, when not split)."""
        location = self.bed_location()
        if not self.bands:
            return [Band(self.name, 1, location * self.body)]
        return [Band(band.name, band.filament, location * band.body) for band in self.bands]

    def compound(self) -> Part | Compound:
        """For the assembly and previews: black, with any colour bands in placeholder colours."""
        if not self.bands:
            self.body.label = self.name
            self.body.color = PRINT_BLACK
            return self.body
        for band in self.bands:
            band.body.label = band.name
            band.body.color = BAND_PREVIEW_COLOURS[(band.filament - 1) % len(BAND_PREVIEW_COLOURS)]
        return Compound(label=self.name, children=[band.body for band in self.bands])
