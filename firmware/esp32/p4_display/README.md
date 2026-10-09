# ESP32-P4 display controller

This is the native display firmware for the Waveshare
**ESP32-P4-WIFI6-Touch-LCD-7B**, 1024 x 600, PCB Rev 1.1. It will replace the
Raspberry Pi application while the existing ESP32-S3 continues to sample the
INMP441 microphone and refresh the HUB75 LED panel.

The firmware initializes the vendor-supported display and GT911 touch
controller, presents the native Waveform One interface, logs the P4 silicon,
flash and PSRAM identity, and acts as a USB CDC host for the existing S3. After
the S3 connects, it requests and verifies Mirrored mode through the WF1
protocol.

## Phone remote and Wi-Fi setup

The ESP32-C6 radio on the Waveshare board is controlled by the P4 over SDIO.
Open **Settings** on the touchscreen, select the user's home network, enter its
password with the on-screen keyboard and press **Connect**. The credentials are
stored in the P4's nonvolatile storage and the device restarts onto that home
network.

The phone remains connected to the same home network. Once the P4 has an IP
address, the Settings screen displays a QR code containing the phone remote's
local URL. The QR opens the app; it is not used to join a device-specific Wi-Fi
network. If the saved network cannot be reached, the touchscreen Wi-Fi controls
remain available so the user can choose a replacement network.

The QR contains a random 128-bit device pairing token in its URL fragment. The
token is generated once, stored in NVS and required by every remote operation;
opening the P4's bare IP address does not authorize controls. The current local
remote uses plain HTTP and is intended only for a trusted home LAN. Production
commissioning must enable flash/NVS encryption before customer Wi-Fi credentials
and the pairing token are treated as protected at rest.

Scan results are retained as structured SSID records rather than reconstructed
from dropdown text. Networks whose names contain control characters, malformed
UTF-8 or text-direction controls are omitted because LVGL cannot present those
names safely. SSIDs containing embedded NUL bytes are also intentionally omitted
because the touchscreen, NVS credential format and ESP-IDF station configuration
use text SSIDs.

After joining the home network, the P4 synchronizes its clock using SNTP. The
clock does not depend on the phone remote being open.

## First-stage wiring

1. Turn off the HUB75 panel supply and unplug the existing ESP32-S3.
2. Leave every HUB75 and INMP441 wire attached to the S3.
3. Do not connect any P4 GPIO header to the LED panel or microphone.
4. Connect the Waveshare board's USB-to-UART USB-C port to the development Mac
   with a data-capable cable.
5. Flash and validate this program before connecting the two controllers.

After display and touch validation, connect a USB-A to USB-C data cable from
the P4 USB host port to the S3 native USB port. The HUB75 panel keeps its
separate regulated 5 V supply. Do not also power the S3 from a second USB host.

## Build

The project uses ESP-IDF and the managed Waveshare 7B BSP. ESP-IDF downloads
the pinned BSP and LVGL components during the first build.

```sh
cd firmware/esp32/p4_display
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/cu.usbserial-PORT flash monitor
```

Use the serial port created by the board's USB-to-UART connector. Exit the
monitor with `Ctrl+]`.

Confirm all of the following before continuing:

- the log reports target `esp32p4`, 32 MB flash and initialized PSRAM;
- note the **silicon revision** printed in the log;
- the native Now Playing and Settings screens render without flicker;
- Settings, brightness, style selection and the Wi-Fi keyboard respond to touch;
- the S3 connects over USB and acknowledges Mirrored mode;
- the P4 joins the selected home network and shows a green Wi-Fi icon, clock
  and phone-remote QR code;
- the QR opens the remote from a phone on the same home network.

`Rev 1.1` printed on the Waveshare PCB is the board revision. It does not
identify the ESP32-P4 silicon revision. This project currently follows
Waveshare's production `rev3_x` profile. Do not select the legacy pre-v3
profile merely because the PCB says Rev 1.1.

## Source provenance

The board configuration and API usage follow Waveshare's official
`ESP32-P4-WIFI6-Touch-LCD-7B` repository at commit
`9fcd49a8c927a00f5583c22d568b3448addbe51c` and managed BSP
`waveshare/esp32_p4_wifi6_touch_lcd_7b` version `3.0.1`. Hardware settings are
consumed through that BSP rather than copied into this repository.
