#!/bin/sh
# Run on the Pi from a checkout containing pi/core and pi/web.
set -eu
cd "$(dirname "$0")/../.."
. "$HOME/.cargo/env"
cargo build --manifest-path pi/core/Cargo.toml --release --locked
install -d "$HOME/.local/bin" "$HOME/.config/systemd/user" "$HOME/.config/waveform-one"
install -m 755 pi/core/target/release/waveform-display "$HOME/.local/bin/waveform-display"
install -m 755 pi/scripts/launch-display.py "$HOME/.local/bin/waveform-kiosk"
cat > "$HOME/.config/systemd/user/waveform-display.service" <<'UNIT'
[Unit]
Description=Waveform One display and remote service
After=network.target

[Service]
ExecStart=%h/.local/bin/waveform-display /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_D4:05:92:7B:9B:7C-if00
Environment=WAVEFORM_BIND=0.0.0.0:8080
Environment=WAVEFORM_BACKLIGHT=/sys/class/backlight/10-0045
Restart=on-failure
RestartSec=3
UMask=0077
NoNewPrivileges=true

[Install]
WantedBy=default.target
UNIT
systemctl --user daemon-reload
systemctl --user enable --now waveform-display.service
systemctl --user restart waveform-display.service
install -d "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/waveform-display.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Waveform One
Exec=$HOME/.local/bin/waveform-kiosk
Terminal=false
DESKTOP
printf 'Display service installed. Open http://raspberrypi.local:8080 on your local network.\n'
