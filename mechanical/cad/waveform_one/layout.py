"""Case size and where each component sits, derived only from parameters.

Positions are the minimum (left, front, bottom) corner of each component body
in world coordinates; see parameters.py for the axis convention.

Front view, left to right: side band | screen | divider | LED panel | side band.
Both components are centred vertically in the band between TOP_BAND and
BOTTOM_BAND.
"""

from waveform_one import parameters as p
from waveform_one.geometry import Vec3


def case_width() -> float:
    return 2 * (p.SHELL_WALL.v + p.SIDE_BAND.v) + p.SCREEN_W.v + p.DIVIDER_WIDTH.v + p.LED_W.v


def component_zone_height() -> float:
    return max(p.SCREEN_H.v, p.LED_H.v)


def case_height() -> float:
    return 2 * p.SHELL_WALL.v + p.TOP_BAND.v + p.BOTTOM_BAND.v + component_zone_height()


def case_size() -> Vec3:
    return (case_width(), p.CASE_DEPTH.v, case_height())


def interior_bounds() -> tuple[Vec3, Vec3]:
    """Space inside the walls, behind the front plate and in front of the rear panel."""
    half = case_width() / 2 - p.SHELL_WALL.v
    return (
        (-half, p.FRONT_THICKNESS.v, p.SHELL_WALL.v),
        (half, p.CASE_DEPTH.v - p.REAR_PANEL_THICKNESS.v, case_height() - p.SHELL_WALL.v),
    )


def zone_centre_z() -> float:
    return p.SHELL_WALL.v + p.BOTTOM_BAND.v + component_zone_height() / 2


def screen_origin() -> Vec3:
    x = -case_width() / 2 + p.SHELL_WALL.v + p.SIDE_BAND.v
    return (x, p.FRONT_THICKNESS.v, zone_centre_z() - p.SCREEN_H.v / 2)


def seam_x() -> float:
    """Centre of the divider between screen and LED panel; the shell splits here."""
    return screen_origin()[0] + p.SCREEN_W.v + p.DIVIDER_WIDTH.v / 2


def diffuser_origin() -> Vec3:
    x = screen_origin()[0] + p.SCREEN_W.v + p.DIVIDER_WIDTH.v
    return (x, p.FRONT_THICKNESS.v, zone_centre_z() - p.LED_H.v / 2)


def led_origin() -> Vec3:
    x, y, z = diffuser_origin()
    return (x, y + p.DIFFUSER_THICKNESS.v, z)


def led_rear_y() -> float:
    return led_origin()[1] + p.LED_D.v


def screen_rear_y() -> float:
    return screen_origin()[1] + p.SCREEN_D.v


def mic_origin() -> Vec3:
    _, (_, y_max, _) = interior_bounds()
    return (
        p.MIC_CENTRE_X.v - p.MIC_BOARD_W.v / 2,
        y_max - p.MIC_BOARD_T.v,
        p.MIC_CENTRE_Z.v - p.MIC_BOARD_H.v / 2,
    )
