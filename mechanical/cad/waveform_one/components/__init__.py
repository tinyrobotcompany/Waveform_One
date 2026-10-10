"""Electronics placeholders, in front-to-back order."""

from waveform_one.components import esp32, hub75, lcd, microphone, raspberry_pi
from waveform_one.geometry import Placeholder


def build_all() -> list[Placeholder]:
    return [
        lcd.build(),
        hub75.build_diffuser(),
        hub75.build_led(),
        raspberry_pi.build(),
        esp32.build(),
        microphone.build(),
    ]
