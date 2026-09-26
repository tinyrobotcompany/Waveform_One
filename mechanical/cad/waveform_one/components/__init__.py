"""Electronics placeholders, in front-to-back order."""

from waveform_one import parameters as p
from waveform_one.components import esp32, hub75, lcd, microphone, raspberry_pi
from waveform_one.geometry import Placeholder


def build_all() -> list[Placeholder]:
    if p.ENCODERS_ENABLED.v:
        raise NotImplementedError("Encoder placeholders are deferred; see components/encoder.py")
    return [
        hub75.build_diffuser(),
        hub75.build_matrix(),
        lcd.build(),
        raspberry_pi.build(),
        esp32.build(),
        microphone.build(),
    ]
