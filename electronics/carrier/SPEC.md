# Waveform One carrier board — revision 1 specification

The carrier board replaces the jumper wiring, the ESP32-S3 dev board and the
Raspberry Pi tray inside the cabinet. The Waveshare ESP32-P4 touchscreen board
stays as bought: it is not modified, only cabled to.

## What it does

1. **Carries the ESP32-S3**, as a soldered ESP32-S3-WROOM-1-N16R8 module (16 MB
   flash, 8 MB PSRAM: the same chip and memory as today). The existing firmware
   runs unchanged.
2. **Drives the LED panel.**
   - Two 74AHCT245 buffers convert the S3's 3.3 V signals to the panel's 5 V.
   - A keyed 2×8 HUB75 socket takes the panel's own ribbon cable.
3. **Connects the microphone.** A locking 6-pin connector and a short cable go to
   the INMP441 on the rear panel.
4. **Takes one power input and shares it out**, with a fuse, surge protection
   and bulk capacitance:
   - LED panel: screw terminal, 4 A.
   - P4 board: 5 V to its USB-C port through a short cable.
   - The S3: on-board 3.3 V regulator.
5. **Links to the P4 over USB, as today.**
   - The S3's native USB goes to a USB-C socket on the board, and a short
     right-angle cable runs to the P4's USB-A host port.
   - The same socket flashes the S3 from a computer.
   - Reset and boot buttons are on the board.
   - Unused pads are kept for a future serial link to the P4's 4-pin UART socket.

## Power budget

| Load | Current at 5 V | Source |
|---|---|---|
| LED panel, full white | 4 A (20 W max) | [Waveshare spec](https://www.waveshare.com/wiki/Template:RGB-Matrix-P4-64x32_Spec) |
| P4 touchscreen board | up to 2 A, budgeted | Not published by [Waveshare](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B); similar boards draw 0.5–1 A |
| ESP32-S3 module and buffers | 0.3 A peak | Espressif module datasheet (radio unused) |
| **Total** | **≈ 6 A (30 W)** | |

## Power input: barrel socket now, USB-C as the target

The 5 V rail feeds everything. Two inputs can supply it:

- **Revision 1 (fitted): 5.5 × 2.1 mm barrel socket**, 5 V / 6 A from a certified
  supply (MEAN WELL GST60A05). It goes through a polyfuse or eFuse, a TVS diode
  (SMAJ5.0A) and 1000 µF of bulk capacitance.
- **USB-C power (target, laid out in revision 1, fitted when proven).**
  - At 5 V, USB-C provides at most 3 A, which is too little. The board must
    instead request a higher voltage with USB Power Delivery: 15 V at 2–3 A
    from a 30–45 W charger.
  - A step-down converter then turns that into 5 V at 6 A.
  - Parts: a USB-C receptacle, a PD sink controller and a 6 A synchronous buck
    converter. They are chosen when the schematic is drawn; candidates are
    checked against current availability.
  - Both inputs join through ideal-diode OR-ing, so either may be used and
    neither back-feeds the other.

The USB cable from the P4 also carries 5 V. The S3's USB socket must not feed
that into the board's rail; only the data lines and detection are connected.

No battery: the product is mains-powered. The P4 board's coin cell keeps its
clock.

## Mechanical

- The board lies flat on the cabinet floor behind the LED panel, near the
  panel's HUB75 input. It is held by four M3 screws into heat-set inserts
  (4.1 mm holes, from the coupon).
- Its outline, mounting holes and connector positions are defined once and
  shared with the casing CAD, so the two cannot drift apart. KiCad exports a 3D
  model of the populated board for collision checks.
- Connectors face the rear and right, away from the LED panel slot.
- The rear panel gets openings for the power input, and for the S3 and P4
  service USB ports.

## Tools

The board is designed in KiCad (10.x). KiCad is widely used in industry and is
the file format the PCB makers (JLCPCB, PCBWay) work from. Its files are
plain text, so design-rule checks and fabrication exports run from the command
line (`kicad-cli`).

## Open questions

- The P4 board's real current draw. The budget above is conservative; a USB
  power meter reading would let the supply shrink.
- The exact PD controller and buck parts (availability at the assembler).
- Whether the P4's onboard microphones can replace the INMP441. That would
  remove the microphone cable, and perhaps later the S3.
