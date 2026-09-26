"""Signed Waveform One release contracts. No publisher credentials on devices."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile
import urllib.request

REPOSITORY = 'tinyrobotcompany/Waveform_One'
BASE = f'https://github.com/{REPOSITORY}/releases'
NAMES = {'pi': 'waveform-one-pi.tar.gz', 'esp': 'waveform-one-esp32s3-ota.bin', 'usb': 'waveform-one-esp32s3-usb.zip'}
LIMITS = {'pi': 256 * 1024 * 1024, 'esp': 3 * 1024 * 1024, 'usb': 8 * 1024 * 1024}


def validate(m, installed_sequence):
    expected = dict(schema=1, product='waveform-one', hardware='esp32s3-16mb',
                    platform='linux-aarch64', python='3.13', protocol=1, channel='stable', source_ref='refs/heads/main')
    if not isinstance(m, dict) or any(m.get(k) != v for k, v in expected.items()):
        raise ValueError('Update is incompatible with this device')
    if type(m.get('sequence')) is not int or not installed_sequence < m['sequence'] < 2**53:
        raise ValueError('Update sequence is not newer than installed release')
    if not re.fullmatch(r'v\d+\.\d+\.\d+', m.get('version', '')):
        raise ValueError('Invalid release version')
    if not re.fullmatch('[0-9a-f]{40}', m.get('commit', '')) or not re.fullmatch('[0-9a-f]{64}', m.get('esp_elf_sha256', '')):
        raise ValueError('Invalid build identity')
    if not isinstance(m.get('notes'), str) or len(m['notes']) > 50000:
        raise ValueError('Invalid release notes')
    if not isinstance(m.get('assets'), dict) or set(m['assets']) != set(NAMES):
        raise ValueError('Missing release components')
    for component, name in NAMES.items():
        a = m['assets'][component]
        if not isinstance(a, dict) or a.get('name') != name or type(a.get('size')) is not int or not 0 < a['size'] <= LIMITS[component]:
            raise ValueError('Invalid asset name or size')
        if not re.fullmatch('[0-9a-f]{64}', a.get('sha256', '')):
            raise ValueError('Invalid asset digest')
    return m


def verify_signature(data, signature, public_key):
    if len(data) > 65536 or len(signature) > 1024:
        raise ValueError('Oversized manifest/signature')
    with tempfile.TemporaryDirectory() as directory:
        d = Path(directory)
        (d/'data').write_bytes(data)
        (d/'signature').write_bytes(signature)
        result = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(public_key),
                                 '-signature', str(d/'signature'), str(d/'data')],
                                capture_output=True, timeout=10)
        if result.returncode:
            raise ValueError('Release signature is invalid')


def verify_asset(data, asset):
    if len(data) != asset['size'] or hashlib.sha256(data).hexdigest() != asset['sha256']:
        raise ValueError('Release asset hash or size mismatch')


class HttpsRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if not newurl.startswith('https://'):
            raise ValueError('Insecure download redirect')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, limit):
    if not url.startswith(BASE + '/'):
        raise ValueError('Unexpected update origin')
    opener = urllib.request.build_opener(HttpsRedirect())
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'Waveform-One-Updater/1'}), timeout=30) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Download exceeds size limit')
    return data


def extract(archive, destination):
    """Validate the entire archive before writing anything; never extract links."""
    destination = Path(destination)
    with tarfile.open(archive, 'r:gz') as tar:
        members = []
        total = 0
        seen = set()
        for m in tar:
            members.append(m)
            p = PurePosixPath(m.name)
            if p.is_absolute() or '..' in p.parts or not p.parts or '\\' in m.name or m.name in seen or not (m.isdir() or m.isfile()):
                raise ValueError('Unsafe release archive member')
            seen.add(m.name)
            total += m.size
            if m.size < 0 or total > 1024**3 or len(members) > 10000:
                raise ValueError('Release archive exceeds extraction limits')
        destination.mkdir(parents=True, exist_ok=False)
        for m in members:
            target = destination / m.name
            if m.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with tar.extractfile(m) as src, target.open('xb') as dst:
                    import shutil
                    shutil.copyfileobj(src, dst)
                target.chmod(0o755 if m.mode & 0o111 else 0o644)


def atomic_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    with temporary.open('w') as f:
        os.chmod(temporary, 0o600)
        json.dump(value, f)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temporary, path)
    sync_directory(path.parent)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def activate(root, release):
    root = Path(root)
    temporary = root/'next'
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(release)
    os.replace(temporary, root/'current')
    sync_directory(root)
