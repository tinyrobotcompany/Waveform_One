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

Revision B ("wide display concept", 2026-10-10): screen and LED panel side by
side behind one black front, inside a rounded cabinet, with a vinyl-sleeve
holder on top. The outer case size is derived from the components and bands
below (see layout.py), not set directly.
"""

from dataclasses import dataclass
from enum import Enum

# Milestone currently being built. Parameters needed only by later milestones
# may stay unset until then.
CURRENT_MILESTONE = 5


class Status(str, Enum):
    """How much a parameter value can be trusted."""

    VERIFIED = "VERIFIED"  # confirmed by a physical fit test
    DATASHEET = "DATASHEET"  # taken from a manufacturer drawing, not yet measured
    PROVISIONAL = "PROVISIONAL"  # design intent or estimate; expected to change
    MEASURED = "MEASURED"  # measured on real hardware (see reference/measurements/)
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


D, P, M, POL = Status.DATASHEET, Status.PROVISIONAL, Status.MEASURED, Status.POLICY
CAL = Status.CALIBRATED

PRINT_MATERIAL = "PETG"
PRINT_REVISION = "B"

# --- Printer: Elegoo Centauri Carbon ----------------------------------------

PRINTER_BUILD_X = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Y = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")
PRINTER_BUILD_Z = Param(256.0, "mm", D, "Centauri Carbon nominal build volume.")

MAX_PRINT_X = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Y = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")
MAX_PRINT_Z = Param(245.0, "mm", POL, "Engineering envelope: margin for brims and bed edges.")
PLATE_DEPTH = Param(225.0, "mm", POL, "Front-to-back room on a print-job plate; the profile keeps ~23 mm at the front for its purge line.")
PLATE_GAP = Param(8.0, "mm", POL, "Space between parts on a print-job plate.")

# --- Cabinet -----------------------------------------------------------------
# A rounded tube (top, bottom, sides) with the black front plate printed as one
# piece, split left/right at the divider between screen and LED panel.

SHELL_WALL = Param(4.0, "mm", P, "Top, bottom and side wall thickness; 4 mm keeps each shell half under a day to print.")
FRONT_THICKNESS = Param(4.0, "mm", P, "Front plate holding both windows.")
CASE_DEPTH = Param(150.0, "mm", P, "Front to rear; room for the Pi behind the screen.")
CORNER_RADIUS = Param(18.0, "mm", P, "Outer radius of the four long edges, as in the concept.")
SIDE_BAND = Param(12.0, "mm", P, "Front between each outer wall and the nearest window component.")
DIVIDER_WIDTH = Param(12.0, "mm", P, "Front band between screen and LED panel; the shell seam runs down its middle.")
TOP_BAND = Param(12.0, "mm", P, "Front above the components; holds the top clip bosses.")
BOTTOM_BAND = Param(12.0, "mm", P, "Front below the components; holds bottom clip bosses and the name.")
FRONT_EDGE_CHAMFER = Param(0.8, "mm", P, "Front perimeter chamfer; also hides first-layer squish.")

FRAME_GROOVE_INSET = Param(11.0, "mm", P, "Groove separating the frame from the black front, from the outer edge.")
FRAME_GROOVE_WIDTH = Param(1.0, "mm", P, "Engraved frame line width.")
FRAME_GROOVE_DEPTH = Param(0.6, "mm", P, "Three 0.2 mm layers.")

FOOT_RECESS_DIAMETER = Param(12.0, "mm", P, "Shallow seats for stick-on rubber feet.")
FOOT_RECESS_DEPTH = Param(1.0, "mm", P, "Seat depth.")
FOOT_INSET = Param(25.0, "mm", P, "Foot centres from the side and front/rear edges.")

# --- Waveshare 7-inch HDMI touchscreen ---------------------------------------
# Connected to the Pi by HDMI (video) and USB (touch).

SCREEN_W = Param(165.0, "mm", M, "Owner measurement (2026-10-10), outer width.")
SCREEN_H = Param(100.0, "mm", M, "Owner measurement (2026-10-10), outer height.")
SCREEN_D = Param(13.0, "mm", P, "Depth including back PCB; measure.")
SCREEN_ACTIVE_W = Param(154.2, "mm", P, "Typical 7-inch 1024x600 visible width; measure.")
SCREEN_ACTIVE_H = Param(86.0, "mm", P, "Typical 7-inch 1024x600 visible height; measure.")
SCREEN_ACTIVE_OFFSET_X = Param(0.0, "mm", P, "Visible area offset from outline centre; assume centred.", signed=True)
SCREEN_ACTIVE_OFFSET_Z = Param(0.0, "mm", P, "Visible area offset from outline centre; assume centred.", signed=True)
SCREEN_WINDOW_MARGIN = Param(1.0, "mm", P, "Window extends this far beyond the visible area.")
SCREEN_REAR_KEEPOUT = Param(30.0, "mm", P, "HDMI and USB plugs plus cable bends behind the screen.")

# --- Waveshare RGB-Matrix-P3-64x32 and the owner's diffuser -------------------

LED_W = Param(195.0, "mm", M, "Owner measurement (2026-10-10), outer width.")
LED_H = Param(95.0, "mm", M, "Owner measurement (2026-10-10), outer height.")
LED_D = Param(12.0, "mm", M, "Owner rough measurement (2026-09-26); excludes connectors.")
LED_WINDOW_LIP = Param(1.0, "mm", P, "Front overlaps the panel edge by this much.")
LED_REAR_KEEPOUT = Param(25.0, "mm", P, "HUB75 plugs, ribbon bend and 5 V lead behind the panel.")
DIFFUSER_THICKNESS = Param(3.0, "mm", P, "Owner's diffuser, assumed panel-sized; measure.")

# --- Component pockets and clips ---------------------------------------------
# Each component drops into a shallow pocket on the back of the front plate and
# is held by four clips screwed into bosses above and below it.

POCKET_CLEARANCE = Param(0.5, "mm", P, "Per side, around each component outline.")
POCKET_WALL = Param(2.0, "mm", P, "Pocket wall thickness.")
POCKET_HEIGHT = Param(6.0, "mm", P, "Pocket wall height behind the front plate.")
CLIP_WIDTH = Param(10.0, "mm", P, "Clip bar width.")
CLIP_THICKNESS = Param(3.0, "mm", P, "Clip bar thickness.")
CLIP_OVERLAP = Param(4.0, "mm", P, "How far a clip reaches over the component edge.")
CLIP_INSET = Param(35.0, "mm", P, "Clip centres from each component's left and right edges.")
BOSS_DIAMETER = Param(8.0, "mm", P, "Heat-set insert bosses.")
KEEPOUT_EDGE_INSET = Param(6.0, "mm", P, "Rear keep-outs stop short of edges where clips sit.")

# --- Fasteners -----------------------------------------------------------------

M3_INSERT_HOLE = Param(4.1, "mm", CAL, "Coupon 2026-10-10: 4.1 mm held the insert best in PETG.", required_from=5)
M3_INSERT_DEPTH = Param(6.0, "mm", P, "Insert length plus a little.")
M3_CLEARANCE_HOLE = Param(3.2, "mm", CAL, "Coupon 2026-10-10: M3 slid through 3.0 mm; 3.2 adds slack to line up holes in two parts.")
M25_SELF_TAP_HOLE = Param(2.4, "mm", P, "M2.5 screws tapped straight into the Pi standoffs.")
JOINT_CLEARANCE = Param(0.25, "mm", P, "Tongue/groove per side; confirm with the coupon.")

# --- Shell halves joint ----------------------------------------------------------

TONGUE_THICKNESS = Param(2.0, "mm", P, "Tongue thickness, centred in the front plate and in the top and bottom walls.")
TONGUE_DEPTH = Param(5.0, "mm", P, "How far the tongue reaches into the other half.")
TONGUE_END_INSET = Param(10.0, "mm", P, "Tongue stops short of the shell's ends.")

# --- Rear panel ----------------------------------------------------------------

REAR_PANEL_THICKNESS = Param(3.0, "mm", POL, "Sits inside the rear opening, flush with the back edge.")
REAR_PANEL_CLEARANCE = Param(0.3, "mm", P, "Per side, around the rear panel.")
REAR_SEAM_X = Param(35.0, "mm", P, "Rear panel split; offset from the shell seam so the panel ties both halves.", signed=True)
REAR_BOSS_WIDTH = Param(10.0, "mm", P, "Rear-panel bosses on top and bottom walls.")
REAR_BOSS_PROTRUSION = Param(9.0, "mm", P, "Boss height off the wall.")
REAR_BOSS_FLAT = Param(8.0, "mm", P, "Full-height length behind the 45-degree print slope.")
REAR_BOSS_X_TOP = (-180.0, -100.0, 0.0, 80.0, 180.0)  # PROVISIONAL: clear of seams and keep-outs
REAR_BOSS_X_BOTTOM = (-125.0, -60.0, 0.0, 80.0, 180.0)  # PROVISIONAL: clear of the Pi's power plug
VENT_SLOT_WIDTH = Param(2.5, "mm", P, "Narrow so the slot tops bridge cleanly.")
VENT_SLOT_LENGTH = Param(36.0, "mm", P, "Slot length.")
VENT_PITCH = Param(6.0, "mm", P, "Slot spacing.")
CABLE_PORT_W = Param(30.0, "mm", P, "Notch in the rear panel's bottom edge for Pi and LED power cables.")
CABLE_PORT_H = Param(14.0, "mm", P, "Notch height.")
CABLE_PORT_X = Param(-150.0, "mm", P, "Behind the Pi's USB-C socket.", signed=True)

# --- Raspberry Pi 4 Model B on its tray ------------------------------------------
# Flat on a tray on the floor behind the screen. USB/Ethernet end faces right
# (+X); the USB-C/micro-HDMI edge faces the rear panel (+Y).

PI_BOARD_L = Param(85.0, "mm", D, "Board length (along X as mounted).")
PI_BOARD_W = Param(56.0, "mm", D, "Board width (along Y as mounted).")
PI_BOARD_T = Param(1.4, "mm", D, "PCB thickness.")
PI_PORT_OVERHANG = Param(3.0, "mm", P, "USB/Ethernet jacks protrude past the board end.")
PI_STANDOFF_HEIGHT = Param(4.0, "mm", P, "Clears through-hole pins under the board.")
PI_STANDOFF_DIAMETER = Param(6.0, "mm", P, "Tray standoffs.")
PI_HEIGHT_ABOVE_BOARD = Param(28.0, "mm", P, "Reserved for a fan-heatsink; owner has no cooler yet.")
PI_HOLE_PITCH_L = Param(58.0, "mm", D, "Mounting-hole spacing along the board length.")
PI_HOLE_PITCH_W = Param(49.0, "mm", D, "Mounting-hole spacing across the board.")
PI_HOLE_INSET = Param(3.5, "mm", D, "Hole centres from the board edges nearest them.")
PI_HOLE_DIAMETER = Param(2.7, "mm", D, "M2.5 clearance holes in the Pi PCB.")
PI_SIDE_GAP = Param(22.0, "mm", P, "Board end to the left wall; keeps the tray on the flat floor.")
PI_USB_PLUG_KEEPOUT = Param(40.0, "mm", P, "USB-A plug (to ESP32) plus cable bend beyond the USB end.")
PI_REAR_PLUG_KEEPOUT = Param(38.0, "mm", P, "USB-C power and micro-HDMI plugs plus bends to the rear panel.")
PI_REAR_PLUG_SPAN = Param(50.0, "mm", P, "Length of the rear edge carrying USB-C and micro-HDMI.")
TRAY_THICKNESS = Param(3.0, "mm", P, "Pi tray plate.")
TRAY_MARGIN = Param(10.0, "mm", P, "Tray extends this far beyond the board.")
TRAY_PAD_SIZE = Param(10.0, "mm", P, "Square floor pads the tray screws onto.")
TRAY_PAD_HEIGHT = Param(4.0, "mm", P, "Floor pad height; deep enough for an insert with the floor.")
TRAY_PAD_INSET = Param(6.0, "mm", P, "Pad centres from the tray corners, clear of the Pi standoffs.")

# --- Espressif ESP32-S3-DevKitC-1 ---------------------------------------------------
# Behind the LED panel, long edge along X, USB end facing left (-X) towards the Pi.

ESP_BOARD_L = Param(63.0, "mm", P, "DevKitC-1 length; confirm against the real board.")
ESP_BOARD_W = Param(25.5, "mm", P, "DevKitC-1 width; confirm against the real board.")
ESP_TOP_HEIGHT = Param(5.0, "mm", P, "PCB plus RF module above the board.")
ESP_UNDERSIDE_KEEPOUT = Param(25.0, "mm", P, "Header pins, Dupont housings and wire bend below.")
ESP_USB_PLUG_KEEPOUT = Param(40.0, "mm", P, "USB-C plug and cable bend beyond the USB end.")
ESP_REAR_OFFSET = Param(6.0, "mm", P, "Gap behind the LED panel's keep-out.")
ESP_GAP_FROM_PI = Param(4.0, "mm", P, "Between the two USB cable keep-outs.")

# --- INMP441 microphone on the rear panel --------------------------------------------

MIC_BOARD_W = Param(15.0, "mm", P, "Breakout width; measure.")
MIC_BOARD_H = Param(15.0, "mm", P, "Breakout height; measure.")
MIC_BOARD_T = Param(3.0, "mm", P, "PCB plus microphone can.")
MIC_WIRE_KEEPOUT = Param(15.0, "mm", P, "Header and I2S wires in front of the board.")
MIC_CENTRE_X = Param(150.0, "mm", P, "Right side, away from the Pi's heat.", signed=True)
MIC_CENTRE_Z = Param(60.0, "mm", P, "Mid-height on the rear panel.")
MIC_POCKET_WALL = Param(1.5, "mm", P, "Locating frame around the breakout.")
MIC_POCKET_HEIGHT = Param(3.0, "mm", P, "Frame height.")
MIC_PORT_DIAMETER = Param(3.0, "mm", P, "Sound hole through the rear panel.")

# --- Name on the front ------------------------------------------------------------------

BRAND_TEXT = "W A V E F O R M   O N E"  # spaced capitals, as in the concept
BRAND_FONT = "Futura"
BRAND_FONT_SIZE = Param(4.4, "mm", P, "Gives ~3.5 mm capitals.")
BRAND_DEPTH = Param(0.6, "mm", P, "Engraved into the front face.")

WINDOW_CHAMFER = Param(1.0, "mm", P, "45-degree chamfer on the front edge of each window.")

# --- Vinyl sleeve holders on top -----------------------------------------------------------

ALBUM_WIDTH = Param(315.0, "mm", D, "Standard 12-inch LP sleeve.")
ALBUM_THICKNESS = Param(3.0, "mm", P, "Reference sleeve thickness (single LP).")
RECORD_SLOT_WIDTH = Param(7.0, "mm", P, "Takes single and gatefold sleeves.")
RECORD_SLOT_DEPTH = Param(18.0, "mm", P, "How far the sleeve sits in the holder.")
RECORD_ANGLE_DEG = Param(8.0, "deg", P, "Rearward lean.")
HOLDER_LENGTH = Param(50.0, "mm", P, "Along X.")
HOLDER_DEPTH = Param(40.0, "mm", P, "Along Y.")
HOLDER_HEIGHT = Param(24.0, "mm", P, "Above the case top.")
HOLDER_CORNER_RADIUS = Param(4.0, "mm", P, "Rounded vertical edges.")
HOLDER_X = Param(110.0, "mm", P, "Holder centres from the middle; supports the sleeve near its edges.")
HOLDER_REAR_INSET = Param(10.0, "mm", P, "Holder rear face from the case's rear face.")
HOLDER_SCREW_SPACING = Param(28.0, "mm", P, "Two screws per holder, along X.")
HOLDER_SCREW_INSET = Param(8.0, "mm", P, "Screw centres from the holder's front face.")

# --- Calibration coupon ------------------------------------------------------------------------

COUPON_INSERT_HOLES = (3.8, 3.9, 4.0, 4.1, 4.2)  # try an insert in each; pick the snug one
COUPON_CLEARANCE_HOLES = (3.0, 3.1, 3.2, 3.3, 3.4)  # pick the smallest an M3 screw slides through
COUPON_PITCH = Param(12.0, "mm", P, "Hole spacing on the coupon.")
COUPON_THICKNESS = Param(8.0, "mm", P, "Deeper than an insert.")


def all_params() -> dict[str, Param]:
    """Every parameter defined in this module, keyed by name."""
    return {name: value for name, value in globals().items() if isinstance(value, Param)}
