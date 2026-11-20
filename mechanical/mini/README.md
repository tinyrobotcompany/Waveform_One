# Waveform One Mini prototype

A narrower vertical enclosure using the current P4 7B display and the current Waveshare P3 64×32 LED panel. Screen above LEDs; no firmware resolution, microphone, LED pin-map or recognition change. This reduces width rather than shrinking either display. It does not include the large enclosure's record sleeve holder.

Default outer envelope: **211.8 × 242.8 × 75 mm (W × H × D)**. Compared with a side-by-side layout using these same component envelopes and margins (403.8 mm wide), this is approximately 47.5% narrower. It is taller. This is a design comparison, not a measured comparison against an assembled production unit.

## Evidence and provisional dimensions

- LED frame: 191.8 × 95.8 mm, 10 mm deep plus 2.7 mm posts; manufacturer drawing recorded in [panel measurements](../reference/measurements/hub75_panel.md).
- P4 7B PCB: manufacturer drawing gives 164 × 97 mm with 156 × 89 mm mounting centres. [Manufacturer dimensions](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B). These are PCB dimensions, not a verified glass outline.
- The **180 × 115 × 22 mm LCD assembly envelope**, 154 × 86 mm visible window, 35 mm cable space, 3 mm wall and 75 mm case depth are provisional design allowances. Measure the actual assembly before printing. The generator rejects invalid dimensions and anything outside the 245 mm engineering print envelope.

## Generate and inspect

Use the existing `mechanical/cad` Python 3.12 environment and build123d dependency:

```sh
mechanical/cad/.venv/bin/python mechanical/mini/design.py /tmp/waveform-mini
python3 -m unittest discover -s mechanical/mini -v
```

The output contains a dimensioned front SVG, parameter/provenance report, STEP and STL for shell, screw-on rear and an adjustable retainer bar (print the required quantity after fitting). Each exported part must be a valid, single positive-volume solid. Generated files in `generated/` are reference prototype outputs, not production-approved manufacturing data.

The shell prints face-down as a single piece; the back and retainers print separately. Four corner bosses use nominal 2.5 mm M3 pilot holes; rear clearance holes are 3.4 mm. Test screw retention in the chosen material and adjust with a coupon before assembly. Retainers have adjustment slots; do not clamp glass or populate mounting holes without measuring the actual stack. Final component mounts, insulation, switch cutout and strain relief must be fitted to the measured assemblies. The rear service opening intentionally avoids assuming exact connector or switch locations. Passive ventilation slots are provided but do not establish thermal compliance.

## BOM delta and validation

Reuse P4/display, LED panel, LED interface PCB, existing suitable power supply and recognition account. Add the smaller printed shell/back, retainer hardware, insulating spacers, rubber feet and the physical mute assembly described by that feature. No component price or ACRCloud discount is assumed; obtain quotes before setting the Mini retail price.

Before production: verify LCD glass/window/depth, microphone acoustic openings, PCB/mount clearances, all cable bend radii, HUB75 noise and strain relief; choose the exact mute switch and add its measured cutout. Assemble unpowered first, then run thermal and stability tests, recognition/LED baselines, offline acceptance and physical mute acceptance. The P4 is disconnected: **no fit, print, thermal, acoustic or electrical validation has been performed**. A genuinely smaller screen/panel would require a separate hardware/UI revision.
