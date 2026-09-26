"""Central engineering parameters for the Waveform One enclosure.

Every dimension used by geometry code lives here, with its unit and how much we
trust it. Geometry modules must read values from this file rather than hard-code
them.

Coordinate convention (stable for the life of the project):
    X = left (-) / right (+), origin at the enclosure's horizontal centre
    Y = front (0) / rear (+), origin on the front-face reference plane
    Z = bottom (0) / top (+), origin on the bottom reference plane
All lengths are millimetres; all angles are degrees.
"""

from dataclasses import dataclass
from enum import Enum


class Status(str, Enum):
    """How much a parameter value can be trusted."""

    VERIFIED = "VERIFIED"  # confirmed by a physical fit test
    DATASHEET = "DATASHEET"  # taken from a manufacturer drawing, not yet measured
    PROVISIONAL = "PROVISIONAL"  # design intent or estimate; expected to change
    MEASURED = "MEASURED"  # caliper-measured on real hardware (see reference/measurements/)
    CALIBRATED = "CALIBRATED"  # established by a printed calibration coupon
    POLICY = "POLICY"  # a deliberate design rule rather than a physical fact


@dataclass(frozen=True)
class Param:
    value: float | None
    unit: str
    status: Status
    note: str


# --- Product envelope -------------------------------------------------------
# Reference volume only; not a printable part.

CASE_WIDTH = Param(330.0, "mm", Status.PROVISIONAL, "Close to 12-inch LP sleeve width (315 mm).")
CASE_DEPTH = Param(145.0, "mm", Status.PROVISIONAL, "PRD nominal depth.")
CASE_HEIGHT = Param(
    265.0,
    "mm",
    Status.PROVISIONAL,
    "Raised from the PRD's 245 mm: the official Pi 7-inch display (~110 mm tall) stacked "
    "above the 64x32 P4 matrix (128 mm) needs ~238 mm before walls and clearances. "
    "Frozen only after the M3 front-layout review.",
)

# --- Printer: Elegoo Centauri Carbon ----------------------------------------

PRINTER_BUILD_X = Param(256.0, "mm", Status.DATASHEET, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Y = Param(256.0, "mm", Status.DATASHEET, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Z = Param(256.0, "mm", Status.DATASHEET, "Centauri Carbon nominal build volume.")

MAX_PRINT_X = Param(245.0, "mm", Status.POLICY, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Y = Param(245.0, "mm", Status.POLICY, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Z = Param(245.0, "mm", Status.POLICY, "Engineering envelope: margin for brims and bed edges.")


def all_params() -> dict[str, Param]:
    """Every parameter defined in this module, keyed by name."""
    return {name: value for name, value in globals().items() if isinstance(value, Param)}
