#!/bin/sh
# Run from the source checkout on the Pi after install-display.sh.
set -eu
cd "$(dirname "$0")/../.."
recognition_dir="$HOME/.local/share/waveform-one"
python3 -m venv "$recognition_dir/recognition-venv"
"$recognition_dir/recognition-venv/bin/pip" install --requirement pi/recognition/requirements.lock
install -m 644 pi/recognition/worker.py "$recognition_dir/recognition-worker.py"
install -d "$HOME/.config/systemd/user"
cat > "$HOME/.config/systemd/user/waveform-recognition.service" <<'UNIT'
[Unit]
Description=Waveform One music recognition (ShazamIO)
After=waveform-display.service network.target

[Service]
ExecStart=%h/.local/share/waveform-one/recognition-venv/bin/python %h/.local/share/waveform-one/recognition-worker.py
Restart=on-failure
RestartSec=10
UMask=0077
NoNewPrivileges=true

[Install]
WantedBy=default.target
UNIT
systemctl --user daemon-reload
systemctl --user enable --now waveform-recognition.service
systemctl --user restart waveform-recognition.service
printf 'Recognition enabled. No API key required.\n'
