#!/bin/sh
# One-time provisioning, after USB migration. Public key and serial path are explicit.
set -eu
if [ "$#" -ne 3 ]; then
  echo 'Usage: sh pi/updater/install.sh PUBLIC_KEY_PEM SERIAL_BY_ID INITIAL_RELEASE_DIRECTORY' >&2
  exit 2
fi
public_key=$1
serial=$2
initial_release=$3
[ "$(uname -m)" = aarch64 ]
python3 -c 'import sys; assert sys.version_info[:2] == (3,13)'
openssl pkey -pubin -in "$public_key" -noout
[ -x "$initial_release/bin/waveform-display" ]
[ -x "$initial_release/venv/bin/python" ]
[ -f "$initial_release/pi/updater/agent.py" ]
[ -f "$initial_release/release.json" ]
[ -e "$serial" ]
root="$HOME/.local/share/waveform-one/releases"
config="$HOME/.config/waveform-one"
units="$HOME/.config/systemd/user"
mkdir -p "$root" "$config" "$units"
[ ! -e "$root/current" ] || { echo 'Updater is already provisioned; refusing to replace recovery state.' >&2; exit 1; }
# Verify that the one-time firmware migration was actually completed.
# Restore services on every exit, including a failed preflight.
trap 'systemctl --user start waveform-display.service waveform-recognition.service' EXIT
systemctl --user stop waveform-recognition.service waveform-display.service
PYTHONPATH="$(dirname "$0")" python3 - "$serial" "$initial_release/release.json" <<'PYCODE'
import json,sys
from esp import connect_healthy
m=json.load(open(sys.argv[2]))
esp=connect_healthy(sys.argv[1],m['esp_elf_sha256'],20)
try: esp.request('WFU CONFIRM','WFU CONFIRMED')
finally: esp.close()
PYCODE
install -m 644 "$public_key" "$config/update-public.pem"
python3 - "$config/updater.json" "$serial" "$root" "$initial_release" <<'PY'
import json,sys
from pathlib import Path
Path(sys.argv[1]).write_text(json.dumps({'serial':sys.argv[2]}))
release=Path(sys.argv[4]).resolve(strict=True)
Path(sys.argv[3],'current').symlink_to(release)
m=json.loads((release/'release.json').read_text())
Path(sys.argv[3],'installed.json').write_text(json.dumps({'sequence':m['sequence'],'version':m['version']}))
PY
# Preserve existing configuration and the stable serial device selected by the owner.
mkdir -p "$units/waveform-display.service.d" "$units/waveform-recognition.service.d"
# Remove only the earlier serial override after recording it. It otherwise overrides ExecStart.
if [ -f "$units/waveform-display.service.d/serial.conf" ]; then
  cp "$units/waveform-display.service.d/serial.conf" "$config/pre-update-serial.conf"
fi
cat > "$units/waveform-display.service.d/zz-updates.conf" <<UNIT
[Service]
ExecStart=
ExecStart=%h/.local/share/waveform-one/releases/current/bin/waveform-display $serial
UNIT
cat > "$units/waveform-recognition.service.d/zz-updates.conf" <<'UNIT'
[Service]
ExecStart=
ExecStart=%h/.local/share/waveform-one/releases/current/venv/bin/python %h/.local/share/waveform-one/releases/current/pi/recognition/worker.py
UNIT
# Future kiosk launches follow the selected release too; current pages reload when
# the server build identity changes. Keep a copy of the pre-update launcher.
mkdir -p "$HOME/.local/bin"
if [ -f "$HOME/.local/bin/waveform-kiosk" ]; then
  cp "$HOME/.local/bin/waveform-kiosk" "$config/pre-update-kiosk"
fi
cat > "$HOME/.local/bin/waveform-kiosk" <<'LAUNCH'
#!/bin/sh
exec /usr/bin/python3 "$HOME/.local/share/waveform-one/releases/current/pi/scripts/launch-display.py"
LAUNCH
chmod 755 "$HOME/.local/bin/waveform-kiosk"
# Keep recovery tooling outside the application being replaced.
cp "$(dirname "$0")/agent.py" "$(dirname "$0")/updates.py" "$(dirname "$0")/esp.py" "$root/"
for action in check install recover; do
  cat > "$units/waveform-update-$action.service" <<UNIT
[Unit]
Description=Waveform One update $action
After=network-online.target
OnFailure=waveform-update-recover.service
[Service]
Type=oneshot
ExecStart=/usr/bin/python3 %h/.local/share/waveform-one/releases/agent.py $action
TimeoutStartSec=15min
UMask=0077
NoNewPrivileges=true
UNIT
done
sed '/OnFailure=/d' "$units/waveform-update-recover.service" > "$units/recover.tmp"
mv "$units/recover.tmp" "$units/waveform-update-recover.service"
cat >> "$units/waveform-update-recover.service" <<'UNIT'
[Install]
WantedBy=default.target
UNIT
cat > "$units/waveform-update-check.timer" <<'UNIT'
[Unit]
Description=Check for Waveform One updates
[Timer]
OnBootSec=5min
OnUnitActiveSec=6h
RandomizedDelaySec=30min
Persistent=true
[Install]
WantedBy=timers.target
UNIT
systemctl --user daemon-reload
systemctl --user enable waveform-update-recover.service
systemctl --user enable --now waveform-update-check.timer
systemctl --user restart waveform-display.service waveform-recognition.service
printf 'Signed update checking enabled. Installation requires Install in Settings.\n'
