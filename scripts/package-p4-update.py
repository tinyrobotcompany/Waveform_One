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
import sys

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/esp32/p4_display'
TARGET = 'waveshare-p4-7b-rev3-32mb'
LAYOUT = 'waveform-ab8-https-v1'
SECURE_LAYOUT = 'waveform-ab8-secure-v1'
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


def verify_secure_image(image, public_key):
    if public_key is None:
        raise ValueError('Production OTA requires the approved secure-boot public key')
    subprocess.run([sys.executable, '-m', 'espsecure', 'verify-signature', '--version', '2',
                    '--keyfile', str(public_key), str(image)], check=True)


def record(build, dist, version, commit, secure_boot_public_key=None):
    blob = (build / 'waveform_p4_display.bin').read_bytes()
    validate_image(blob, version)
    config = (build / 'config/sdkconfig.h').read_text()
    if '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1' not in config:
        raise ValueError('Rollback must be enabled')
    for header in [build / 'config/sdkconfig.h', build / 'bootloader/config/sdkconfig.h']:
        if '#define CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH 1' not in header.read_text():
            raise ValueError(f'32-bit flash addressing must be enabled in {header}')
    secure = '#define CONFIG_WAVEFORM_PRODUCTION_SECURITY 1' in config
    if secure:
        verify_secure_image(build / 'waveform_p4_display.bin', secure_boot_public_key)
    elif secure_boot_public_key is not None:
        raise ValueError('Secure-boot key supplied for a development image')
    credentials = (build / 'esp-idf/main/acrcloud_config.h').read_text()
    for name in ['HOST', 'ACCESS_KEY', 'ACCESS_SECRET']:
        if not re.search(r'^#define ACRCLOUD_' + name + r' ""$', credentials, re.MULTILINE):
            raise ValueError('Public OTA images must use device ACRCloud credentials, not embed them')
    dist.mkdir(parents=True, exist_ok=True)
    (dist / IMAGE).write_bytes(blob)
    shutil.copyfile(APP / 'dependencies.lock', dist / 'p4-dependencies.lock')
    (dist / 'p4-update-build.json').write_text(json.dumps({
        'commit': commit, 'version': version, 'sha256': digest(dist / IMAGE),
        'layout': SECURE_LAYOUT if secure else LAYOUT,
        'dependency_lock_sha256': digest(dist / 'p4-dependencies.lock'),
    }, sort_keys=True) + '\n')


def verify_build(dist, version, commit):
    provenance = json.loads((dist / 'p4-update-build.json').read_text())
    if (provenance['commit'] != commit or
            provenance['version'] != version or provenance['sha256'] != digest(dist / IMAGE) or
            provenance['dependency_lock_sha256'] != digest(dist / 'p4-dependencies.lock')):
        raise ValueError('P4 artifacts do not match the validated source/version')
    blob = (dist / IMAGE).read_bytes()
    validate_image(blob, version)
    return provenance, blob


def package(dist, plan, commit, key, public_key, secure_boot_public_key=None):
    if plan['commit'] != commit:
        raise ValueError('Release plan does not match the validated source')
    version = plan['tag'].removeprefix('v')
    provenance, blob = verify_build(dist, version, commit)
    layout = provenance.get('layout', LAYOUT)
    if layout not in (LAYOUT, SECURE_LAYOUT):
        raise ValueError('Unknown P4 layout')
    if layout == SECURE_LAYOUT:
        verify_secure_image(dist / IMAGE, secure_boot_public_key)
    elif secure_boot_public_key is not None:
        raise ValueError('Secure-boot key supplied for a development image')
    blob = (dist / IMAGE).read_bytes()
    validate_image(blob, version)
    manifest = dict(schema=1, source_commit=commit, version=version, target=TARGET,
                    layout=layout, size=len(blob), sha256=hashlib.sha256(blob).hexdigest(),
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
    parser.add_argument('action', choices=['record', 'verify', 'package'])
    parser.add_argument('--dist', required=True, type=Path)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--secure-boot-public-key', type=Path)
    parser.add_argument('--plan', type=Path)
    args = parser.parse_args()
    if args.action == 'record':
        if not args.build or not args.version:
            parser.error('record requires --build and --version')
        record(args.build, args.dist, args.version, os.environ['GITHUB_SHA'], args.secure_boot_public_key)
    else:
        if not args.plan:
            parser.error('package requires --plan')
        if args.action == 'verify':
            plan = json.loads(args.plan.read_text())
            if plan['commit'] != os.environ['GITHUB_SHA']:
                raise ValueError('Release plan commit mismatch')
            verify_build(args.dist, plan['tag'].removeprefix('v'), os.environ['GITHUB_SHA'])
            return
        key = os.environ.get('UPDATE_SIGNING_KEY', '')
        if not key:
            raise ValueError('UPDATE_SIGNING_KEY is required')
        with tempfile.TemporaryDirectory() as temp:
            private = Path(temp) / 'signing.pem'
            with open(private, 'w', opener=lambda path, flags: os.open(path, flags, 0o600)) as output:
                output.write(key)
            options = {} if args.secure_boot_public_key is None else {'secure_boot_public_key': args.secure_boot_public_key}
            package(args.dist, json.loads(args.plan.read_text()), os.environ['GITHUB_SHA'], private,
                    APP / 'main/ota_public.pem', **options)


if __name__ == '__main__':
    main()
