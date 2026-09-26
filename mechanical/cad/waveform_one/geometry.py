"""Small geometry helpers shared by component and enclosure modules."""

from dataclasses import dataclass, field

from build123d import Align, Box, Color, Compound, Location, Part, Pos

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

    def on_bed(self) -> Part:
        posed = self.print_pose * self.body
        box = posed.bounding_box()
        return Pos(-box.center().X, -box.center().Y, -box.min.Z) * posed

    def compound(self) -> Part:
        self.body.label = self.name
        self.body.color = PRINT_BLACK
        return self.body
