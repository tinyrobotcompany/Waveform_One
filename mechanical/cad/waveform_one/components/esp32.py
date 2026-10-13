"""Espressif ESP32-S3-DevKitC-1 placeholder behind the LED panel, USB end left (-X)."""

from build123d import Color

from waveform_one import layout
from waveform_one import parameters as p
from waveform_one.geometry import Placeholder, box

NAME = "ESP32"


def build() -> Placeholder:
    x, y, z = layout.esp_origin()
    length, width = p.ESP_BOARD_L.v, p.ESP_BOARD_W.v
    (_, _, z_min), _ = layout.interior_bounds()
    usb_l = p.ESP_USB_PLUG_KEEPOUT.v
    underside = box((x, y, z_min), (length, width, z - z_min))
    usb = box((x - usb_l, y, z_min), (usb_l, width, z - z_min + p.ESP_TOP_HEIGHT.v))
    return Placeholder(
        NAME,
        box((x, y, z), (length, width, p.ESP_TOP_HEIGHT.v)),
        Color(0.15, 0.15, 0.2),
        keepouts={"HeadersAndHub75Wires": underside, "UsbFromPi": usb},
    )
