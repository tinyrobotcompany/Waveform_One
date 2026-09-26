# Pi control integration

First integration slice: a Rust USB controller, using the architecture's native
Pi runtime. Audio capture, noise calibration and panel refresh stay on the ESP.
This is a command-line controller, not yet the persistent daemon or Qt/QML UI.

Tested target: Raspberry Pi 4, 64-bit Debian 13, ESP32-S3 native USB Serial/JTAG.
The prototype uses versioned text commands; the planned binary/CRC protocol and
PCM streaming remain future work. See [protocol](../protocol/specification.md).

## Wiring

Power the Pi through its normal USB-C supply. Connect a Pi USB-A port to the
ESP's existing USB connector using a data cable. Keep the panel on its own
existing 5 V supply and preserve the common-ground wiring. The Pi's micro-HDMI
ports connect displays, not the ESP. No extra GPIO wires are needed for control.

## Build and use

Install Rust (tested with 1.98.1) and a C linker/compiler. From the repository:

```sh
cargo test --manifest-path pi/core/Cargo.toml --locked
cargo build --manifest-path pi/core/Cargo.toml --release --locked
ls -l /dev/serial/by-id/
pi/core/target/release/waveform-control /dev/ttyACM0 interactive
```

Use the stable `/dev/serial/by-id/...` path instead of `/dev/ttyACM0` when multiple
USB devices are attached. The user must belong to `dialout`; do not run the
controller as root. Only one program should own the serial port at a time.
Close the controller before flashing or running a monitor.

At `waveform>` type `classic`, `mirrored`, `waterfall`, `status`, or `quit`.
For a single change, replace `interactive` with a mode name. The controller
reports success only after the ESP acknowledges that request. It times out
after three seconds when the firmware lacks the protocol or the device is
unavailable; it does not silently claim a mode changed.

Settings are volatile. ESP reboot returns to classic and starts three-second
noise calibration. Keep music paused for five seconds following a reboot.
The controller avoids deliberately toggling reset lines; USB opening behaviour
can still depend on the OS/adapter. Disconnecting the Pi leaves the ESP running
its current mode as long as it still has power.

GitHub releases include an ARM64 Linux controller archive, built on Ubuntu 24.04
(glibc 2.39 or later required). Extract it and run the executable as above. It is
not an auto-update installer. Native source builds support other compatible Pi OS
versions. The 7-inch screen's Qt/QML control surface is the next integration step.
