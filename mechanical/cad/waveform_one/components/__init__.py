"""Electronics placeholders, in front-to-back order."""

from waveform_one.components import carrier, hub75, lcd, led_adapter, microphone
from waveform_one.geometry import Placeholder


def build_all() -> list[Placeholder]:
    return [
        lcd.build(),
        hub75.build_diffuser(),
        hub75.build_led(),
        carrier.build(),
        led_adapter.build(),
        microphone.build(),
    ]
