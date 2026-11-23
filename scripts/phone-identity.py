#!/usr/bin/env python3
"""Offline USB provisioning for a unique Waveform One HTTPS identity. Never contacts a CA.

Private material is created only in an explicitly supplied private directory. Keys are generated on a trusted provisioning computer, one per device. Protect
and remove workstation copies according to the manufacturing key-custody policy.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys

OPENSSL = os.environ.get("WAVEFORM_OPENSSL") or next((str(p) for p in (Path("/opt/homebrew/opt/openssl@3/bin/openssl"), Path("/usr/local/opt/openssl@3/bin/openssl")) if p.is_file()), "openssl")


def run(*args, data=None):
    if args[0] == "openssl": args = (OPENSSL, *args[1:])
    result = subprocess.run(args, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise ValueError('Identity operation failed: ' + args[0] + ' ' + args[1])
    return result.stdout


def hostname(value):
    if len(value) > 96 or not re.fullmatch(r'[a-z0-9](?:[a-z0-9.-]*[a-z0-9])?', value):
        raise ValueError('Use a lowercase DNS hostname, at most 96 characters.')
    labels = value.split('.')
    if len(labels) < 2 or all(label.isdigit() for label in labels) or any(
        not label or len(label) > 63 or label.startswith('-') or label.endswith('-') for label in labels
    ):
        raise ValueError('Use a DNS hostname, not an IP address or wildcard.')
    return value


def private_directory(path):
    path = path.resolve()
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if path.stat().st_mode & 0o077:
        raise ValueError('Identity directory must have permissions 0700.')
    return path


def create(directory, name):
    name = hostname(name)
    directory = private_directory(directory)
    if any(directory.iterdir()):
        raise ValueError('Use an empty directory; existing identities are never overwritten.')
    run('openssl', 'genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-256', '-pkeyopt', 'ec_param_enc:named_curve', '-out', str(directory/'device.key'))
    (directory/'device.key').chmod(0o600)
    run('openssl', 'req', '-new', '-sha256', '-key', str(directory/'device.key'), '-subj', '/CN='+name,
        '-addext', 'subjectAltName=DNS:'+name, '-out', str(directory/'device.csr'))
    (directory/'identity.json').write_text(json.dumps({'schema': 1, 'hostname': name})+'\n')


def development_certificate(directory):
    """Private test CA, distinct from the device key. Never a production issuer."""
    name = hostname(json.loads((directory/'identity.json').read_text())['hostname'])
    if (directory/'ca.key').exists() or (directory/'fullchain.pem').exists():
        raise ValueError('Refusing to replace an existing CA or certificate.')
    run('openssl', 'req', '-x509', '-newkey', 'rsa:3072', '-nodes', '-sha256', '-days', '365',
        '-subj', '/CN=Waveform One development trust ONLY', '-addext', 'basicConstraints=critical,CA:TRUE,pathlen:0',
        '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-addext', 'subjectKeyIdentifier=hash',
        '-keyout', str(directory/'ca.key'), '-out', str(directory/'ca.pem'))
    (directory/'ca.key').chmod(0o600)
    extensions = directory/'extensions.cnf'
    extensions.write_text('subjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:'+name+'\n')
    run('openssl', 'x509', '-req', '-in', str(directory/'device.csr'), '-CA', str(directory/'ca.pem'),
        '-CAkey', str(directory/'ca.key'), '-CAcreateserial', '-days', '30', '-sha256',
        '-extfile', str(extensions), '-out', str(directory/'fullchain.pem'))


def certificate_blocks(path):
    if not 0 < path.stat().st_size < 8192:
        raise ValueError('Certificate bundle exceeds firmware limits')
    data=path.read_bytes()
    pattern=rb'-----BEGIN CERTIFICATE-----\s+[A-Za-z0-9+/=\r\n]+-----END CERTIFICATE-----'
    blocks=re.findall(pattern,data)
    if not blocks or re.sub(pattern,b'',data).strip():
        raise ValueError('Certificate bundle contains malformed or extra PEM material')
    return blocks


def validate(directory, trust):
    certificates=certificate_blocks(directory/'fullchain.pem')
    roots=certificate_blocks(trust)
    if len(certificates)+len(roots)>16:
        raise ValueError('Too many certificates for firmware validity tracking')
    with tempfile.TemporaryDirectory(dir=directory) as tmp:
        temporary=Path(tmp)
        # Parse every block: OpenSSL otherwise ignores trailing malformed/extra PEMs.
        for index,block in enumerate(certificates+roots):
            candidate=temporary/('certificate-'+str(index)+'.pem')
            candidate.write_bytes(block+b'\n')
            run('openssl','x509','-in',str(candidate),'-noout')
        leaf=temporary/'certificate-0.pem'
        intermediates=None
        if len(certificates)>1:
            intermediates=temporary/'intermediates.pem'
            intermediates.write_bytes(b'\n'.join(certificates[1:])+b'\n')
        return validate_leaf(directory, trust, leaf, intermediates)


def validate_leaf(directory, trust, chain, intermediates):
    name = hostname(json.loads((directory/'identity.json').read_text())['hostname'])
    key = directory/'device.key'
    if key.stat().st_mode & 0o077:
        raise ValueError('Private key must have permissions 0600.')
    if not 0 < chain.stat().st_size < 8192 or not 0 < key.stat().st_size < 4096 or not 0 < trust.stat().st_size < 8192:
        raise ValueError('Identity exceeds firmware limits.')
    cert_public = run('openssl', 'x509', '-in', str(chain), '-pubkey', '-noout')
    key_public = run('openssl', 'pkey', '-in', str(key), '-pubout')
    if cert_public != key_public:
        raise ValueError('Certificate does not match this device private key.')
    subjects = run('openssl', 'x509', '-in', str(chain), '-noout', '-issuer', '-subject', '-nameopt', 'RFC2253').decode().splitlines()
    if len(subjects) != 2 or subjects[0].partition('=')[2] == subjects[1].partition('=')[2]:
        raise ValueError('Device certificates must be issued by a separate CA')
    constraints = run('openssl', 'x509', '-in', str(chain), '-noout', '-ext', 'basicConstraints').decode()
    if 'CA:FALSE' not in constraints:
        raise ValueError('A non-CA device leaf certificate is required')
    sans = run('openssl', 'x509', '-in', str(chain), '-noout', '-ext', 'subjectAltName').decode()
    names = re.findall(r'DNS:([^,\s]+)', sans)
    if names != [name]:
        raise ValueError('Require exactly one DNS identity; shared/wildcard certificates are rejected.')
    run('openssl', 'x509', '-in', str(chain), '-checkend', '86400', '-noout')
    arguments = ['openssl', 'verify', '-purpose', 'sslserver', '-verify_hostname', name, '-CAfile', str(trust)]
    if intermediates is not None: arguments += ['-untrusted', str(intermediates)]
    run(*arguments, str(chain))
    return name


def image(directory, trust, idf, nvs_key=None):
    name = validate(directory, trust)
    generator = idf/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'
    if not generator.is_file():
        raise ValueError('Set --idf to the ESP-IDF installation.')
    with tempfile.TemporaryDirectory(dir=directory) as tmp:
        source = Path(tmp)/'identity.csv'
        with source.open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerows([['key','type','encoding','value'],['identity','namespace','',''],
                             ['hostname','data','string',name],
                             ['cert','file','string',str((directory/'fullchain.pem').resolve())],
                             ['trust','file','string',str(trust.resolve())],
                             ['key','file','string',str((directory/'device.key').resolve())]])
        if nvs_key is None:
            run(sys.executable, str(generator), 'generate', str(source), str(directory/'phone_tls.bin'), '0x10000')
        else:
            if nvs_key.stat().st_mode & 0o077 or nvs_key.stat().st_size != 4096:
                raise ValueError('Production NVS key partition must be a private 4096-byte file.')
            run(sys.executable, str(generator), 'encrypt', str(source), str(directory/'phone_tls.bin'), '0x10000', '--inputkey', str(nvs_key.resolve()))
    (directory/'phone_tls.bin').chmod(0o600)


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['create','dev-cert','validate','image'])
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--hostname', type=hostname)
    parser.add_argument('--nvs-key', type=Path, help='Factory per-device NVS key partition; encrypt identity before first boot')
    parser.add_argument('--trust', type=Path, help='Trusted CA PEM (development CA or public CA bundle)')
    parser.add_argument('--idf', type=Path, default=Path(os.environ.get('IDF_PATH', Path.home()/'.espressif/v6.1/esp-idf')))
    args = parser.parse_args()
    try:
        if args.action == 'create':
            if not args.hostname: raise ValueError('--hostname is required.')
            create(args.directory, args.hostname)
        else:
            directory = private_directory(args.directory)
            if args.action == 'dev-cert': development_certificate(directory)
            else:
                if not args.trust: raise ValueError('--trust is required; verification cannot be disabled.')
                if args.action == 'validate': validate(directory, args.trust)
                else: image(directory, args.trust, args.idf, args.nvs_key)
        print('Identity operation completed. Private material remains in the selected directory.')
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
