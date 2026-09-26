# Kovi scrolling message — standalone fun demo

Displays `I Love Kovi K` in magenta, scrolling right to left repeatedly.
Uses a 5x7 font enlarged to 10x14 pixels and advances one pixel every 40 ms.
No microphone, network, or music input is needed.

Uses the working direct ESP32-S3 to 64x32 HUB75 wiring:
RGB1 = GPIO4/8/9, RGB2 = GPIO10/11/12, ABCD = GPIO13/14/15/16,
CLK = GPIO17, LAT = GPIO18, OE = GPIO21. Panel remains externally powered
at 5 V with common ground. No rewiring is needed from the working LED test.

In an activated ESP-IDF terminal, from this directory:

```sh
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash
```

Exit any running monitor with Ctrl-] first. Replace the port if it changed.
The message runs automatically after flashing. Power the panel before the
ESP starts so the panel receives its startup sequence.

To restore the existing wiring diagnostic:

```sh
cd ../led_test
idf.py -p /dev/cu.usbmodem1101 flash
```

The demo reuses the working diagnostic's GPIO scan and startup sequence;
the panel driver IC model has not been independently identified. Hardware
appearance must be checked on the panel after flashing.
