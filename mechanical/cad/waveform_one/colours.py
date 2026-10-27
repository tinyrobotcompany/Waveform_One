"""The case colours, from mechanical/colours.toml.

Each colour fills one slicer filament slot; the bands of a printed part name
their slot (geometry.Band.filament).
"""

import re
import tomllib
from pathlib import Path

from build123d import Color

COLOURS_FILE = Path(__file__).resolve().parents[2] / "colours.toml"
# Slot order: geometry.Band.filament 1..4 (body, the two stripes, the name inlay).
SLOTS = ("body", "stripe_1", "stripe_2", "name")
HEX = re.compile(r"#[0-9A-Fa-f]{6}")


def load(path: Path = COLOURS_FILE) -> tuple[str, ...]:
    """The colours in slot order, as upper-case "#RRGGBB"."""
    values = tomllib.loads(path.read_text())
    unknown = set(values) - set(SLOTS)
    missing = [slot for slot in SLOTS if slot not in values]
    if unknown or missing:
        raise ValueError(f"{path.name}: expected exactly {', '.join(SLOTS)}; missing {missing}, unknown {sorted(unknown)}")
    for slot in SLOTS:
        if not isinstance(values[slot], str) or not HEX.fullmatch(values[slot]):
            raise ValueError(f'{path.name}: {slot} = {values[slot]!r} is not a colour like "#C12E1F"')
    return tuple(values[slot].upper() for slot in SLOTS)


def filaments(slot_colours: tuple[str, ...], used: set[int] | None = None) -> tuple[list[str], dict[int, int]]:
    """One filament per distinct colour: (their colours, slot -> filament), both numbered from 1.

    Slots of the same colour share a filament, so the printer needs one spool
    per colour, not one per slot. Only the `used` slots (all if None) count.
    Filaments follow slot order, so the body's comes first.
    """
    slots = [slot for slot in range(1, len(slot_colours) + 1) if used is None or slot in used]
    distinct = list(dict.fromkeys(slot_colours[slot - 1] for slot in slots))
    return distinct, {slot: distinct.index(slot_colours[slot - 1]) + 1 for slot in slots}


def preview(hex_colour: str) -> Color:
    """A build123d colour for previews and the STEP assembly."""
    return Color(*(int(hex_colour[i : i + 2], 16) / 255 for i in (1, 3, 5)))


def rgb(hex_colour: str) -> tuple[int, int, int]:
    return tuple(int(hex_colour[i : i + 2], 16) for i in (1, 3, 5))
