#!/usr/bin/env python3
"""Per-user, outbound-only signed application updater for commissioned Pi devices."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import shutil
import time
import urllib.request

from updates import (BASE, NAMES, activate, atomic_json, download, extract,
                     sync_directory, validate, verify_asset, verify_signature)
from esp import Esp, connect_healthy
from policy import automatic_install, mode

CONFIG=Path.home()/'.config/waveform-one'
ROOT=Path.home()/'.local/share/waveform-one/releases'
SERVICES=['waveform-recognition.service','waveform-display.service']


def read(path, default=None):
    return json.loads(path.read_text()) if path.exists() else default


def status(phase, message, **extra):
    selected=read(CONFIG/'updater.json',{}).get('deployment_mode','production')
    atomic_json(CONFIG/'update-status.json',dict(phase=phase,message=message,deployment_mode=selected,**extra))


def service(action):
    subprocess.run(['systemctl','--user',action,*SERVICES],check=True,timeout=30)


def health(timeout=25, expected_build=None):
    token=(CONFIG/'remote-token').read_text().strip()
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            req=urllib.request.Request('http://127.0.0.1:8080/api/state',headers={'Authorization':f'Bearer {token}'})
            with urllib.request.urlopen(req,timeout=2) as r: value=json.load(r)
            if value['device']['connected'] and (expected_build is None or value.get('build')==expected_build):return
        except (OSError,ValueError,KeyError):pass
        time.sleep(1)
    raise RuntimeError('Updated display service did not reconnect to ESP')


def candidate(installed):
    # The signature authenticates exact bytes; validate only after authentication.
    data=download(BASE+'/latest/download/update.json',65536)
    signature=download(BASE+'/latest/download/update.sig',1024)
    verify_signature(data,signature,CONFIG/'update-public.pem')
    m=json.loads(data)
    if m.get('sequence')==installed.get('sequence') and m.get('version')==installed.get('version'):
        return None
    return validate(m,installed.get('sequence',0))


def recover(config):
    journal=read(ROOT/'transaction.json')
    if not journal:return
    installed=read(ROOT/'installed.json',{})
    if installed.get('sequence')==journal['sequence']:
        (ROOT/'transaction.json').unlink();sync_directory(ROOT);return
    if mode(config)=='development':
        atomic_json(ROOT/'failed-automatic.json',{'sequence':journal['sequence']})
    status('recovering','Restoring the previous release…')
    service('stop')
    activate(ROOT,journal['previous'])
    try:
        # Both versions speak protocol 1. Never flash the recovery image blindly.
        esp=Esp(config['serial'])
        try:
            current=esp.status()
            if journal['esp_changed'] and current['digest']==journal['esp_digest']:
                esp.request('WFU ROLLBACK','WFU ROLLING_BACK')
            else:
                esp.request('WFU CANCEL','WFU CANCELLED')
        finally:esp.close()
    finally:
        service('start')
    health(45)
    (ROOT/'transaction.json').unlink();sync_directory(ROOT)
    status('failed','The update was interrupted or failed. The previous release has been restored.')


def install(m,config):
    version=m['version']
    if shutil.disk_usage(ROOT).free < 2*1024**3:
        raise RuntimeError('At least 2 GB free space is required to stage and retain a recovery release')
    release=Path(tempfile.mkdtemp(prefix=version+'-',dir=ROOT))
    release.rmdir()
    status('downloading','Downloading and verifying the update…',version=version)
    assets={}
    for component,asset in m['assets'].items():
        if component == 'usb':continue  # Factory migration is never an OTA action.
        data=download(BASE+f'/download/{version}/'+asset['name'],asset['size'])
        verify_asset(data,asset)
        assets[component]=data
    archive=ROOT/'download.tar.gz'
    archive.write_bytes(assets['pi'])
    extract(archive,release)
    atomic_json(release/'release.json',m)
    archive.unlink()
    # A release-local venv permits dependency rollback without touching the old one.
    subprocess.run([sys.executable,'-m','venv',str(release/'venv')],check=True,timeout=60)
    subprocess.run([str(release/'venv/bin/pip'),'install','--no-index','--find-links',str(release/'wheels'),
                    '-r',str(release/'pi/recognition/requirements.lock')],check=True,timeout=240)
    subprocess.run([str(release/'venv/bin/python'),'-c','import shazamio'],check=True,timeout=30)
    # Flush staged files before a durable transaction points at them.
    os.sync()
    previous=str((ROOT/'current').resolve(strict=True))
    journal=dict(previous=previous,sequence=m['sequence'],esp_digest=m['esp_elf_sha256'],esp_changed=False)
    atomic_json(ROOT/'transaction.json',journal)
    status('installing','Installing update. The display will reconnect shortly.',version=version)
    time.sleep(1)  # Let connected screens display the restart message before USB handover.
    service('stop')
    esp=None
    try:
        esp=Esp(config['serial'])
        old=esp.status()
        journal['esp_changed']=old['digest']!=m['esp_elf_sha256']
        atomic_json(ROOT/'transaction.json',journal)
        if journal['esp_changed']:
            esp.transfer(assets['esp'],m['assets']['esp']['sha256'],
                         lambda p:status('installing',f'Updating LED controller: {p}%',version=version))
        esp.close();esp=None
        esp=connect_healthy(config['serial'],m['esp_elf_sha256'])
        esp.close();esp=None
        activate(ROOT,release)
        service('start')
        health(expected_build=m['commit'])
        # Confirm only after both the new ESP and the Pi service pass health checks.
        service('stop')
        esp=connect_healthy(config['serial'],m['esp_elf_sha256'],15)
        esp.request('WFU CONFIRM','WFU CONFIRMED')
        esp.close();esp=None
        service('start')
        health(expected_build=m['commit'])
        atomic_json(ROOT/'installed.json',dict(sequence=m['sequence'],version=version,previous=previous))
        (ROOT/'transaction.json').unlink();sync_directory(ROOT)
        status('current','Waveform One is up to date.',version=version)
    except Exception:
        if esp:esp.close()
        recover(config)
        raise


def prune():
    # Only directories created by this updater with an authenticated manifest marker.
    # Keep the current version and its immediate predecessor for recovery.
    if (ROOT/'transaction.json').exists():return
    retained={(ROOT/'current').resolve()}
    previous=read(ROOT/'installed.json',{}).get('previous')
    if previous:retained.add(Path(previous).resolve())
    for path in ROOT.iterdir():
        if path.is_symlink() or not path.is_dir() or path.resolve() in retained:continue
        marker=read(path/'release.json',{})
        if marker.get('product')=='waveform-one' and path.name.startswith(marker.get('version','INVALID')+'-'):
            shutil.rmtree(path)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('action',choices=['check','install','recover','sync-main'])
    args=parser.parse_args()
    CONFIG.mkdir(parents=True,exist_ok=True);ROOT.mkdir(parents=True,exist_ok=True)
    with (ROOT/'lock').open('w') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:return
        try:
            config=read(CONFIG/'updater.json')
            if not config or not (CONFIG/'update-public.pem').exists():
                status('unconfigured','Updates need one-time device setup.');return
            if platform.machine()!='aarch64' or sys.version_info[:2]!=(3,13):
                raise RuntimeError('Updater requires ARM64 Raspberry Pi OS with Python 3.13')
            automatic=automatic_install(config,args.action)
            recover(config)
            if args.action=='recover':return
            prune()
            installed=read(ROOT/'installed.json',{})
            m=candidate(installed)
            if not m:
                status('current','Waveform One is up to date.',version=installed.get('version'));return
            if args.action=='check':
                status('available','An update is available.',version=m['version'],notes=m['notes']);return
            if automatic:
                if read(ROOT/'failed-automatic.json',{}).get('sequence')==m['sequence']:
                    status('failed','This development release failed. Automatic retry is paused; check and install manually to retry.',version=m['version'])
                    return
                try:
                    install(m,config)
                except Exception:
                    atomic_json(ROOT/'failed-automatic.json',{'sequence':m['sequence']})
                    raise
                return
            # Install the exact version the user saw, never a newer unreviewed release.
            requested=read(CONFIG/'update-request.json',{})
            if requested.get('version')!=m['version']:raise RuntimeError('Available version changed; check updates and select Install again')
            install(m,config)
        except Exception as error:
            status('failed',str(error)[:400])
            raise

if __name__=='__main__':main()
