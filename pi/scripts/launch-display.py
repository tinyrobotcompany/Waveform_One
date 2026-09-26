#!/usr/bin/env python3
"""Launch the Pi kiosk with its local pairing token; never print the token."""
import os
from pathlib import Path
import time
import urllib.request

config = Path.home() / '.config/waveform-one'
for _ in range(60):
    try:
        with urllib.request.urlopen('http://127.0.0.1:8080/', timeout=1):
            if (config / 'remote-token').exists():
                break
    except OSError:
        pass
    time.sleep(0.5)
else:
    raise SystemExit('Waveform display service did not become ready')
token = (config / 'remote-token').read_text().strip()
os.environ.setdefault('WAYLAND_DISPLAY', 'wayland-0')
os.execvp('chromium', ['chromium', '--ozone-platform=wayland', '--kiosk',
    '--no-first-run', '--noerrdialogs', '--password-store=basic',
    '--user-data-dir=' + str(config / 'chromium'),
    'http://127.0.0.1:8080/?kiosk=1#token=' + token])
