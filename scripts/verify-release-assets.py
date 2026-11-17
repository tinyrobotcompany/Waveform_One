#!/usr/bin/env python3
"""Validate the complete Pi/S3/P4 publication set before upload and after download."""
import argparse
import hashlib
from pathlib import Path

LEGACY_ASSETS = frozenset({
    'waveform-control-linux-aarch64.tar.gz',
    'waveform-control-linux-aarch64.tar.gz.sha256',
    'waveform-one-pi.tar.gz', 'pi-update-build.json',
    'waveform-one-esp32s3-ota.bin', 'waveform-one-esp32s3-usb.zip',
    'esp-update-build.json', 'SHA256SUMS', 'update.json', 'update.sig',
})
P4_ASSETS = frozenset({
    'waveform-p4.bin', 'waveform-p4-manifest.json', 'waveform-p4-manifest.sig',
    'P4-SHA256SUMS', 'p4-dependencies.lock', 'p4-update-build.json',
    'p4-release.json', 'p4-notes.md',
})
RELEASE_ASSETS = LEGACY_ASSETS | P4_ASSETS


def validate_staging(directory):
    names = {path.name for path in directory.iterdir()}
    if names != RELEASE_ASSETS:
        raise ValueError(f'Release asset set mismatch: missing={sorted(RELEASE_ASSETS - names)}, '
                         f'unexpected={sorted(names - RELEASE_ASSETS)}')
    for name in sorted(RELEASE_ASSETS):
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ValueError('Release asset must be a regular file: ' + name)


def digest(path):
    with path.open('rb') as source:
        result = hashlib.sha256()
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            result.update(chunk)
        return result.digest()


def verify_assets(expected, downloaded):
    validate_staging(expected)
    validate_staging(downloaded)
    for name in sorted(RELEASE_ASSETS):
        if digest(expected / name) != digest(downloaded / name):
            raise ValueError('Draft asset differs from validated artifact: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dist', required=True, type=Path)
    parser.add_argument('--downloaded', type=Path)
    args = parser.parse_args()
    if args.downloaded is None:
        validate_staging(args.dist)
    else:
        verify_assets(args.dist, args.downloaded)


if __name__ == '__main__':
    main()
