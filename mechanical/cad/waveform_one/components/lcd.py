"""Waveshare ESP32-P4-WIFI6-Touch-LCD-7B placeholder, glass against the front plate."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "Screen"


def active_area() -> tuple[tuple[float, float], tuple[float, float]]:
    """((x_min, x_max), (z_min, z_max)) of the visible area."""
    x, _, z = layout.screen_origin()
    cx = x + p.SCREEN_W.v / 2 + p.SCREEN_ACTIVE_OFFSET_X.v
    cz = z + p.SCREEN_H.v / 2 + p.SCREEN_ACTIVE_OFFSET_Z.v
    half_w, half_h = p.SCREEN_ACTIVE_W.v / 2, p.SCREEN_ACTIVE_H.v / 2
    return (cx - half_w, cx + half_w), (cz - half_h, cz + half_h)


def edge_x(along: float) -> float:
    """World X of a point `along` mm from the board's outer (4-pin) end, which faces the case's left side."""
    x, _, _ = layout.screen_origin()
    return x + (p.SCREEN_W.v - p.P4_PCB_W.v) / 2 + along


def gpio_plug_keepouts() -> dict[str, object]:
    """Room behind the two 12-pin GPIO sockets for the plugs and their wires."""
    _, _, z = layout.screen_origin()
    top, rear = z + p.SCREEN_H.v, layout.screen_rear_y()
    margin = p.GPIO_PLUG_MARGIN.v
    return {
        f"GpioPlug{index}": box(
            (edge_x(start) - margin, rear, top - p.GPIO_PLUG_KEEPOUT_HEIGHT.v),
            (end - start + 2 * margin, p.GPIO_PLUG_KEEPOUT_DEPTH.v, p.GPIO_PLUG_KEEPOUT_HEIGHT.v),
        )
        for index, (start, end) in enumerate(p.P4_GPIO_SOCKETS, start=1)
    }


def build() -> Placeholder:
    x, y, z = layout.screen_origin()
    w, d, h = p.SCREEN_W.v, p.SCREEN_D.v, p.SCREEN_H.v
    (ax0, ax1), (az0, az1) = active_area()
    marker_t = 0.2  # visual marker only, sitting in the window opening
    active = box((ax0, y - marker_t, az0), (ax1 - ax0, marker_t, az1 - az0))
    inset = p.KEEPOUT_EDGE_INSET.v
    rear = box((x + inset, y + d, z + inset), (w - 2 * inset, p.SCREEN_REAR_KEEPOUT.v, h - 2 * inset))
    return Placeholder(
        NAME,
        box((x, y, z), (w, d, h)),
        Color(0.25, 0.28, 0.35),
        keepouts={"HdmiAndUsb": rear, **gpio_plug_keepouts()},
        markers={"ActiveArea": active},
    )
