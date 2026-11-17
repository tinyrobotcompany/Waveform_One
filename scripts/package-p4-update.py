#!/usr/bin/env python3
"""Tempo-style signed P4 OTA assets, bound to this run's validated build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/esp32/p4_display'
TARGET = 'waveshare-p4-7b-rev3-32mb'
LAYOUT = 'waveform-ab8-model-v1'
IMAGE = 'waveform-p4.bin'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_image(blob, version):
    if not re.fullmatch(r'(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})', version):
        raise ValueError('Invalid release version')
    if not 1024 <= len(blob) <= 0x800000 or blob[0] != 0xe9:
        raise ValueError('Invalid ESP application image or OTA slot overflow')
    if (int.from_bytes(blob[32:36], 'little') != 0xabcd5432 or
            blob[48:80].split(b'\0')[0] != version.encode() or
            blob[80:112].split(b'\0')[0] != b'waveform_p4_display'):
        raise ValueError('Image descriptor does not match the P4 release')
    if re.search(rb'-----BEGIN (?:EC |RSA )?PRIVATE KEY-----[\r\n]+[A-Za-z0-9+/=\r\n]{64,}-----END (?:EC |RSA )?PRIVATE KEY-----', blob):
        raise ValueError('Private key material in release image')


def record(build, dist, version, commit):
    blob = (build / 'waveform_p4_display.bin').read_bytes()
    validate_image(blob, version)
    config = (build / 'config/sdkconfig.h').read_text()
    if '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1' not in config:
        raise ValueError('Rollback must be enabled')
    dist.mkdir(parents=True, exist_ok=True)
    (dist / IMAGE).write_bytes(blob)
    shutil.copyfile(APP / 'dependencies.lock', dist / 'p4-dependencies.lock')
    (dist / 'p4-update-build.json').write_text(json.dumps({
        'commit': commit, 'version': version, 'sha256': digest(dist / IMAGE),
        'dependency_lock_sha256': digest(dist / 'p4-dependencies.lock'),
    }, sort_keys=True) + '\n')


def package(dist, plan, commit, key, public_key):
    version = plan['tag'].removeprefix('v')
    provenance = json.loads((dist / 'p4-update-build.json').read_text())
    if (plan['commit'] != commit or provenance['commit'] != commit or
            provenance['version'] != version or provenance['sha256'] != digest(dist / IMAGE) or
            provenance['dependency_lock_sha256'] != digest(dist / 'p4-dependencies.lock')):
        raise ValueError('P4 artifacts do not match the validated source/version')
    blob = (dist / IMAGE).read_bytes()
    validate_image(blob, version)
    manifest = dict(schema=1, source_commit=commit, version=version, target=TARGET,
                    layout=LAYOUT, size=len(blob), sha256=hashlib.sha256(blob).hexdigest(),
                    dependency_lock_sha256=provenance['dependency_lock_sha256'],
                    url=f'https://github.com/tinyrobotcompany/Waveform_One/releases/download/v{version}/{IMAGE}')
    path = dist / 'waveform-p4-manifest.json'
    path.write_text(json.dumps(manifest, sort_keys=True, separators=(',', ':')) + '\n')
    signature = dist / 'waveform-p4-manifest.sig'
    subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(key), '-out', str(signature), str(path)], check=True)
    subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(public_key), '-signature', str(signature), str(path)], check=True)
    (dist / 'p4-release.json').write_text(json.dumps(plan, indent=2) + '\n')
    (dist / 'p4-notes.md').write_text(plan['notes'])
    names = [IMAGE, path.name, signature.name, 'p4-dependencies.lock', 'p4-release.json', 'p4-notes.md']
    (dist / 'P4-SHA256SUMS').write_text(''.join(f'{digest(dist / name)}  {name}\n' for name in names))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['record', 'package'])
    parser.add_argument('--dist', required=True, type=Path)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--plan', type=Path)
    args = parser.parse_args()
    if args.action == 'record':
        if not args.build or not args.version:
            parser.error('record requires --build and --version')
        record(args.build, args.dist, args.version, os.environ['GITHUB_SHA'])
    else:
        if not args.plan:
            parser.error('package requires --plan')
        key = os.environ.get('UPDATE_SIGNING_KEY', '')
        if not key:
            raise ValueError('UPDATE_SIGNING_KEY is required')
        with tempfile.TemporaryDirectory() as temp:
            private = Path(temp) / 'signing.pem'
            with private.open('w', opener=lambda path, flags: os.open(path, flags, 0o600)) as output:
                output.write(key)
            package(args.dist, json.loads(args.plan.read_text()), os.environ['GITHUB_SHA'], private,
                    APP / 'main/ota_public.pem')


if __name__ == '__main__':
    main()
