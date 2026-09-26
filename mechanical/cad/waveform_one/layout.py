"""Where each component sits, derived only from parameters.

Positions are the minimum (left, front, bottom) corner of each component body
in world coordinates; see parameters.py for the axis convention.
"""

from waveform_one import parameters as p
from waveform_one.geometry import Vec3


def interior_bounds() -> tuple[Vec3, Vec3]:
    """Space inside the walls, front bezel and rear panel."""
    half = p.CASE_WIDTH.v / 2 - p.WALL_THICKNESS.v
    return (
        (-half, p.FRONT_BEZEL_THICKNESS.v, p.WALL_THICKNESS.v),
        (half, p.CASE_DEPTH.v - p.REAR_PANEL_THICKNESS.v, p.CASE_HEIGHT.v - p.WALL_THICKNESS.v),
    )


def diffuser_origin() -> Vec3:
    return (-p.MATRIX_W.v / 2, p.FRONT_BEZEL_THICKNESS.v, p.MATRIX_BOTTOM_Z.v)


def matrix_origin() -> Vec3:
    y = p.FRONT_BEZEL_THICKNESS.v + p.DIFFUSER_THICKNESS.v + p.DIFFUSER_TO_MATRIX_GAP.v
    return (-p.MATRIX_W.v / 2, y, p.MATRIX_BOTTOM_Z.v)


def matrix_rear_y() -> float:
    return matrix_origin()[1] + p.MATRIX_D.v


def display_origin() -> Vec3:
    z = p.MATRIX_BOTTOM_Z.v + p.MATRIX_H.v + p.DISPLAY_MATRIX_GAP.v
    return (-p.DISPLAY_W.v / 2, p.FRONT_BEZEL_THICKNESS.v + p.DISPLAY_RECESS.v, z)


def pi_body_size() -> Vec3:
    return (
        p.PI_BOARD_L.v + p.PI_PORT_OVERHANG.v,
        p.PI_BOARD_W.v,
        p.PI_UNDERSIDE_CLEARANCE.v + p.PI_BOARD_T.v + p.PI_HEIGHT_ABOVE_BOARD.v,
    )


def pi_origin() -> Vec3:
    (x_min, _, _), (_, y_max, _) = interior_bounds()
    y = y_max - p.PI_REAR_PLUG_KEEPOUT.v - p.PI_BOARD_W.v
    return (x_min + p.PI_USB_PLUG_KEEPOUT.v, y, p.PI_BASE_Z.v)


def esp_origin() -> Vec3:
    (x_min, _, z_min), _ = interior_bounds()
    y = matrix_rear_y() + p.MATRIX_REAR_KEEPOUT.v + p.ESP_REAR_OFFSET.v
    return (x_min + p.ESP_USB_PLUG_KEEPOUT.v, y, z_min + p.ESP_UNDERSIDE_KEEPOUT.v)


def mic_origin() -> Vec3:
    _, (_, y_max, _) = interior_bounds()
    return (
        p.MIC_CENTRE_X.v - p.MIC_BOARD_W.v / 2,
        y_max - p.MIC_BOARD_T.v,
        p.MIC_CENTRE_Z.v - p.MIC_BOARD_H.v / 2,
    )
