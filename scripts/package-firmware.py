#!/usr/bin/env python3
"""Package a USB flash bundle, not an OTA image or signed update."""
import argparse
import hashlib
import json
from pathlib import Path
import os
import subprocess
import zipfile


def image_identity(image, elf):
    # ESP32-S3 image: 24-byte header + 8-byte first segment header, then
    # esp_app_desc_t; app_elf_sha256 is at descriptor offset 144 (IDF 6.1).
    if len(image)<288 or image[0]!=0xe9 or int.from_bytes(image[32:36],'little')!=0xabcd5432:
        raise ValueError('Missing ESP application descriptor')
    embedded=bytes(image[176:208])
    if embedded!=hashlib.sha256(elf).digest():
        raise ValueError('Firmware image identity does not match the built ELF')
    return embedded.hex()

def bundle(build, output, commit):
    build = build.resolve()
    config = json.loads((build / 'flasher_args.json').read_text())
    if config['extra_esptool_args']['chip'] != 'esp32s3':
        raise ValueError('Expected ESP32-S3 firmware')
    files = {}
    for name in list(config['flash_files'].values()) + ['flash_args', 'flasher_args.json']:
        path = (build / name).resolve()
        if build not in path.parents:
            raise ValueError('Flash file must be inside build directory')
        files[name] = path.read_bytes()
    manifest = {'product': 'waveform-one', 'target': 'esp32s3', 'commit': commit,
                'otaReady': False, 'installation': 'USB / esptool',
                'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    files['INSTALL.txt'] = b'''Waveform One ESP32-S3 USB bundle
Unzip into a new directory. Stop idf.py monitor before flashing.
From this directory, with ESP-IDF v6.1 tools active:
  esptool --chip esp32s3 --port YOUR_SERIAL_PORT write-flash @flash_args
This replaces the firmware and partition table. Use only the documented
Waveform One ESP32-S3 16 MB hardware and wiring. This is NOT an OTA package.
SHA256SUMS detects download corruption; it is not a cryptographic signature.
'''
    output.mkdir(parents=True, exist_ok=True)
    # Application-only image for provisioned A/B devices; never OTA the USB ZIP.
    app = build / 'waveform_visualizer.bin'
    elf = build / 'waveform_visualizer.elf'
    if app.exists() and elf.exists():
        (output / 'waveform-one-esp32s3-ota.bin').write_bytes(app.read_bytes())
    archive = output / 'waveform-one-esp32s3-usb.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    (output / 'SHA256SUMS').write_text(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
    if app.exists() and elf.exists():
        names=['waveform-one-esp32s3-ota.bin',archive.name]
        (output / 'esp-update-build.json').write_text(json.dumps({
            'commit':commit,'esp_elf_sha256':image_identity(app.read_bytes(),elf.read_bytes()),
            'sha256':{name:hashlib.sha256((output/name).read_bytes()).hexdigest() for name in names}}))



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    commit = os.environ.get('GITHUB_SHA') or subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    bundle(args.build, args.output, commit)
