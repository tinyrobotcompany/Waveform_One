#!/usr/bin/env python3
"""Prepare a verified first install. Does not flash, stop services or activate it."""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import sys
from updates import BASE,validate,verify_signature,download,verify_asset,extract,atomic_json

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('public_key',type=Path);p.add_argument('destination',type=Path)
    args=p.parse_args()
    if platform.machine()!='aarch64' or sys.version_info[:2]!=(3,13):
        raise RuntimeError('Provision on ARM64 Raspberry Pi OS with Python 3.13')
    raw=download(BASE+'/latest/download/update.json',65536)
    sig=download(BASE+'/latest/download/update.sig',1024)
    verify_signature(raw,sig,args.public_key)
    m=validate(json.loads(raw),0)
    args.destination.mkdir(parents=True,exist_ok=False)
    for component in ['pi','usb']:
        a=m['assets'][component];data=download(BASE+f"/download/{m['version']}/"+a['name'],a['size'])
        verify_asset(data,a);(args.destination/a['name']).write_bytes(data)
    release=args.destination/'application'
    extract(args.destination/m['assets']['pi']['name'],release)
    subprocess.run([sys.executable,'-m','venv',str(release/'venv')],check=True)
    subprocess.run([str(release/'venv/bin/pip'),'install','--no-index','--find-links',str(release/'wheels'),
                    '-r',str(release/'pi/recognition/requirements.lock')],check=True)
    atomic_json(release/'release.json',m)
    print(f"Verified {m['version']}; application and USB migration bundle prepared at {args.destination}.")
    print('No services were changed. Perform the documented one-time USB migration before enabling updates.')
