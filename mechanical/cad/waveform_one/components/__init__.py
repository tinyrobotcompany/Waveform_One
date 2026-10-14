"""Electronics placeholders, in front-to-back order."""

from waveform_one.components import carrier, hub75, lcd, microphone
from waveform_one.geometry import Placeholder


def build_all() -> list[Placeholder]:
    return [
        lcd.build(),
        hub75.build_diffuser(),
        hub75.build_led(),
        carrier.build(),
        microphone.build(),
    ]
