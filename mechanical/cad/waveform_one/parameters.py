"""Central engineering parameters for the Waveform One enclosure.

Every dimension used by geometry code lives here, with its unit and how much we
trust it. Geometry modules must read values from this file rather than hard-code
them.

Coordinate convention (stable for the life of the project):
    X = left (-) / right (+), origin at the enclosure's horizontal centre
    Y = front (0) / rear (+), origin on the front-face reference plane
    Z = bottom (0) / top (+), origin on the bottom reference plane
All lengths are millimetres; all angles are degrees.

Component placeholders are built in their mounted orientation, and their sizes
below are given as world-axis extents: *_W along X, *_D along Y, *_H along Z.
"""

from dataclasses import dataclass
from enum import Enum

# Milestone currently being built. Parameters needed only by later milestones
# may stay unset until then.
CURRENT_MILESTONE = 3


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
    required_from: int = 0  # first milestone that needs a value
    signed: bool = False  # offsets may be zero or negative

    @property
    def v(self) -> float:
        if self.value is None:
            raise ValueError(f"Parameter is unset ({self.status.value}): {self.note}")
        return self.value


D, P, POL, C = Status.DATASHEET, Status.PROVISIONAL, Status.POLICY, Status.CALIBRATED

# --- Product envelope -------------------------------------------------------
# Reference volume only; not a printable part.

CASE_WIDTH = Param(330.0, "mm", P, "Close to 12-inch LP sleeve width (315 mm).")
CASE_DEPTH = Param(145.0, "mm", P, "PRD nominal depth.")
CASE_HEIGHT = Param(245.0, "mm", P, "PRD nominal height; frozen after the M3 front-layout review.")

# --- Printer: Elegoo Centauri Carbon ----------------------------------------

PRINTER_BUILD_X = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Y = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Z = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")

MAX_PRINT_X = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Y = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Z = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")

# --- Shell -------------------------------------------------------------------

WALL_THICKNESS = Param(3.0, "mm", POL, "PRD minimum general wall; sides, top and bottom.")
FRONT_BEZEL_THICKNESS = Param(4.0, "mm", POL, "PRD nominal front bezel.")
REAR_PANEL_THICKNESS = Param(3.0, "mm", POL, "PRD nominal rear panel.")

# --- Front layout ------------------------------------------------------------
# Display stacked above the matrix, both centred in X (encoders deferred).

MATRIX_BOTTOM_Z = Param(14.0, "mm", P, "Bottom wall plus bezel lip below the matrix window.")
DISPLAY_MATRIX_GAP = Param(12.0, "mm", P, "Bezel band separating display and matrix windows.")
MIN_TOP_MARGIN = Param(10.0, "mm", POL, "Least material above the display: top wall plus bezel lip.")
MIN_SIDE_MARGIN = Param(10.0, "mm", POL, "Least material beside the widest front component.")

# --- Official Raspberry Pi 7-inch Touchscreen (original, DSI) ----------------

DISPLAY_W = Param(194.0, "mm", D, "Published outer width; measure the real unit.")
DISPLAY_H = Param(110.0, "mm", D, "Published outer height; measure the real unit.")
DISPLAY_D = Param(20.0, "mm", D, "Published depth including rear standoffs.")
DISPLAY_ACTIVE_W = Param(155.0, "mm", D, "Published visible area width.")
DISPLAY_ACTIVE_H = Param(86.0, "mm", D, "Published visible area height.")
DISPLAY_ACTIVE_OFFSET_X = Param(0.0, "mm", P, "Visible-area offset from glass centre; assume centred until measured.", signed=True)
DISPLAY_ACTIVE_OFFSET_Z = Param(0.0, "mm", P, "Visible-area offset from glass centre; assume centred until measured.", signed=True)
DISPLAY_RECESS = Param(1.0, "mm", P, "Glass sits this far behind the bezel's rear face.")
DISPLAY_CABLE_KEEPOUT_W = Param(90.0, "mm", P, "DSI ribbon and power leads behind the adapter board.")
DISPLAY_CABLE_KEEPOUT_D = Param(15.0, "mm", P, "Depth reserved behind the display for ribbon bend.")
DISPLAY_CABLE_KEEPOUT_H = Param(60.0, "mm", P, "Height of the ribbon/lead zone.")

# --- Waveshare RGB-Matrix-P3-64x32 (HUB75) -----------------------------------

MATRIX_W = Param(192.0, "mm", D, "64 px x 3 mm pitch. Owner's rough measurement: ~195 mm.")
MATRIX_H = Param(96.0, "mm", D, "32 px x 3 mm pitch. Owner's rough measurement: ~100 mm.")
MATRIX_D = Param(12.0, "mm", P, "Owner's rough measurement; excludes connectors.")
MATRIX_CLEARANCE = Param(0.5, "mm", P, "PRD fit clearance around the panel.")
MATRIX_REAR_KEEPOUT = Param(25.0, "mm", P, "HUB75 IDC plugs, ribbon bend and 5 V lead behind the panel.")

# --- LED diffuser (owner-supplied) ------------------------------------------

DIFFUSER_THICKNESS = Param(3.0, "mm", P, "Owner already has a diffuser; measure it.")
DIFFUSER_TO_MATRIX_GAP = Param(3.0, "mm", P, "Air gap that blends pixels; tune once the diffuser is measured.")

# --- Raspberry Pi 4 Model B --------------------------------------------------
# Mounted flat. Long edge along X with USB/Ethernet end facing left (-X), so
# the USB-C power and micro-HDMI edge faces the rear panel (+Y).

PI_BOARD_L = Param(85.0, "mm", D, "Board length (along X as mounted).")
PI_BOARD_W = Param(56.0, "mm", D, "Board width (along Y as mounted).")
PI_BOARD_T = Param(1.4, "mm", D, "PCB thickness.")
PI_PORT_OVERHANG = Param(3.0, "mm", P, "USB/Ethernet jacks protrude past the board end.")
PI_UNDERSIDE_CLEARANCE = Param(4.0, "mm", P, "Through-hole pins and standoffs below the board.")
PI_HEIGHT_ABOVE_BOARD = Param(28.0, "mm", P, "Reserved for a fan-heatsink; owner has no cooler yet.")
PI_HOLE_PITCH_L = Param(58.0, "mm", D, "Mounting-hole spacing along the board length.")
PI_HOLE_PITCH_W = Param(49.0, "mm", D, "Mounting-hole spacing across the board.")
PI_HOLE_INSET = Param(3.5, "mm", D, "Hole centres from the board edges nearest them.")
PI_HOLE_DIAMETER = Param(2.7, "mm", D, "M2.5 clearance holes in the Pi PCB.")
PI_BASE_Z = Param(125.0, "mm", P, "Tray height: upper rear, near the display's DSI ribbon.")
PI_USB_PLUG_KEEPOUT = Param(40.0, "mm", P, "USB-A plug (to ESP32) plus cable bend beyond the USB end.")
PI_REAR_PLUG_KEEPOUT = Param(38.0, "mm", P, "USB-C power plug plus bend between the Pi and rear panel.")
PI_REAR_PLUG_SPAN = Param(50.0, "mm", P, "Length of the rear edge carrying USB-C and micro-HDMI.")

# --- Espressif ESP32-S3-DevKitC-1 --------------------------------------------
# Mounted flat behind the matrix, long edge along X, USB end facing left (-X).

ESP_BOARD_L = Param(63.0, "mm", P, "DevKitC-1 length; confirm against the real board.")
ESP_BOARD_W = Param(25.5, "mm", P, "DevKitC-1 width; confirm against the real board.")
ESP_TOP_HEIGHT = Param(5.0, "mm", P, "PCB plus RF module above the board.")
ESP_UNDERSIDE_KEEPOUT = Param(25.0, "mm", P, "Header pins, Dupont housings and wire bend below.")
ESP_USB_PLUG_KEEPOUT = Param(40.0, "mm", P, "USB-C plug and cable bend beyond the USB end.")
ESP_REAR_OFFSET = Param(6.0, "mm", P, "Gap between the matrix keep-out and the board.")

# --- INMP441 microphone breakout --------------------------------------------
# On the rear panel's inner face, sound port facing out through the panel.

MIC_BOARD_W = Param(15.0, "mm", P, "Breakout width; measure.")
MIC_BOARD_H = Param(15.0, "mm", P, "Breakout height; measure.")
MIC_BOARD_T = Param(3.0, "mm", P, "PCB plus microphone can.")
MIC_WIRE_KEEPOUT = Param(15.0, "mm", P, "Header and I2S wires in front of the board.")
MIC_CENTRE_X = Param(100.0, "mm", P, "Right side: away from the Pi's heat and the HUB75 ribbon.")
MIC_CENTRE_Z = Param(55.0, "mm", P, "Low on the rear panel, keeping I2S wires to the ESP32 short.")

# --- M3 front-layout mock-up ------------------------------------------------
# A flat front panel printed face-down in three pieces, joined by rear splice
# plates and stood upright on two slotted feet. For judging proportions only.

PRINT_MATERIAL = "PLA"
PRINT_REVISION = "A"

DISPLAY_WINDOW_MARGIN = Param(1.0, "mm", P, "Window extends this far beyond the visible area on each side.")
MATRIX_WINDOW_LIP = Param(1.0, "mm", P, "Bezel overlaps the panel edge by this much on each side.")
WINDOW_CHAMFER = Param(1.0, "mm", P, "45-degree chamfer on the front edge of each window.")
FRONT_SEAM_MARGIN = Param(12.0, "mm", P, "Seams sit this far outside the widest front component.")
MIN_SIDE_PIECE_WIDTH = Param(30.0, "mm", POL, "Narrowest side strip worth printing and handling.")

BRAND_TEXT_LEFT = "WAVEFORM ONE"
BRAND_TEXT_RIGHT = "MARK I"
BRAND_FONT = "Futura"  # bold; macOS system font
BRAND_DEPTH = Param(0.6, "mm", P, "Engraved into the front face: three 0.2 mm layers.")
BRAND_SIDE_MARGIN = Param(6.0, "mm", P, "Clear band either side of the text within each side strip.")
BRAND_MAX_FONT_SIZE = Param(8.0, "mm", POL, "Upper limit when fitting the text to the strip width.")
BRAND_MIN_CAP_HEIGHT = Param(3.5, "mm", POL, "Smallest engraved lettering treated as legible on a 0.4 mm nozzle.")

SPLICE_WIDTH = Param(20.0, "mm", P, "Rear plate bridging each seam, glued on.")
SPLICE_THICKNESS = Param(2.0, "mm", P, "Rear plate thickness.")
SPLICE_END_MARGIN = Param(20.0, "mm", P, "Splice stops short of the top and bottom edges.")

STAND_WIDTH = Param(40.0, "mm", P, "Along X; each stand straddles a seam.")
STAND_FRONT = Param(20.0, "mm", P, "Stand length in front of the panel.")
STAND_REAR = Param(60.0, "mm", P, "Stand length behind the panel, resisting a leaning LP.")
STAND_HEIGHT = Param(20.0, "mm", P, "Overall stand height.")
STAND_SLOT_DEPTH = Param(12.0, "mm", P, "How far the panel sits into the stand.")
STAND_SLOT_CLEARANCE = Param(0.4, "mm", P, "Total slot play; uncalibrated, shim with tape if loose.")

ALBUM_THICKNESS = Param(3.0, "mm", P, "Reference sleeve thickness (single LP).")
ALBUM_REAR_OFFSET = Param(20.0, "mm", P, "Sleeve's lower edge sits this far in front of the rear face.")

# --- LP sleeve support (M5) --------------------------------------------------

ALBUM_WIDTH = Param(315.0, "mm", D, "Standard 12-inch LP sleeve.")
ALBUM_SLOT_WIDTH = Param(318.0, "mm", P, "PRD slot width.")
ALBUM_SLOT_DEPTH = Param(10.0, "mm", P, "PRD slot depth.")
ALBUM_ANGLE_DEG = Param(8.0, "deg", P, "PRD rearward lean.")

# --- Rotary encoders (deferred) ----------------------------------------------
# Controls are touchscreen and phone for now. Values kept so encoders can return
# without redesign; no front-panel holes are cut while ENCODERS_ENABLED is 0.

ENCODERS_ENABLED = Param(0, "flag", POL, "Owner deferred physical knobs (2026-09-26).")
ENCODER_COUNT = Param(3, "count", P, "PRD value if re-enabled.")
ENCODER_SPACING = Param(38.0, "mm", P, "PRD value if re-enabled.")
ENCODER_SHAFT_DIAMETER = Param(6.0, "mm", D, "Bourns PEC11R D-shaft.")

# --- Joints and fasteners (calibrated in M4) ----------------------------------

JOINT_CLEARANCE = Param(0.25, "mm", P, "Tongue/groove per side; M4 coupon replaces this.")
ALIGNMENT_PIN_CLEARANCE = Param(0.20, "mm", P, "M4 coupon replaces this.")
M3_CLEARANCE_HOLE = Param(3.2, "mm", P, "M4 coupon replaces this.")
M3_INSERT_HOLE = Param(None, "mm", C, "Heat-set insert hole; set from the M4 coupon.", required_from=5)
GENERAL_COMPONENT_CLEARANCE = Param(0.5, "mm", P, "PRD general loose fit.")


def all_params() -> dict[str, Param]:
    """Every parameter defined in this module, keyed by name."""
    return {name: value for name, value in globals().items() if isinstance(value, Param)}
