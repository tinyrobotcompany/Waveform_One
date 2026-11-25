"""Ephemeral CA fixtures for the real firmware installer test; never device provisioning."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)

def run(*args):
    result = subprocess.run(['openssl', *args], cwd=root, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return result.stdout

def request(name):
    run('req', '-new', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256', '-pkeyopt', 'ec_param_enc:named_curve', '-sha256',
        '-nodes', '-keyout', name+'.key', '-out', name+'.csr', '-subj', '/CN='+name)

def authority(name):
    run('req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256', '-pkeyopt', 'ec_param_enc:named_curve', '-sha256', '-nodes',
        '-keyout', name+'.key', '-out', name+'.pem', '-days', '7', '-subj', '/CN='+name,
        '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign')

def sign(name, issuer, csr, extensions, days='3'):
    run('x509', '-sha256', '-req', '-in', csr, '-CA', issuer+'.pem', '-CAkey', issuer+'.key',
        '-CAcreateserial', '-days', days, '-extfile', extensions, '-out', name+'.pem')

if sys.argv[2] == 'prepare':
    authority('root')
    authority('rogue')
    request('intermediate')
    (root/'intermediate.ext').write_text('basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\n')
    sign('intermediate', 'root', 'intermediate.csr', 'intermediate.ext', '5')
    section = '.section __TEXT,__const' if sys.platform == 'darwin' else '.section .rodata'
    path = str(root/'root.pem').replace('\\', '\\\\').replace('"', '\\"')
    (root/'roots.S').write_text(f'{section}\n.globl _binary_device_roots_pem_start\n_binary_device_roots_pem_start:\n.incbin "{path}"\n.byte 0\n.globl _binary_device_roots_pem_end\n_binary_device_roots_pem_end:\n')
elif sys.argv[2] == 'check-csr':
    name = (root/'hostname.txt').read_text()
    if len(sys.argv) > 3:
        assert len(name) == int(sys.argv[3]), (len(name), name)
    # Independent OpenSSL parsing plus signature verification of the actual CSR.
    text = run('req', '-in', 'device.csr', '-verify', '-noout', '-text')
    subject = run('req', '-in', 'device.csr', '-noout', '-subject', '-nameopt', 'RFC2253').strip()
    assert subject.removeprefix('subject=').strip() == 'CN='+name, subject
    sans = re.findall(r'X509v3 Subject Alternative Name:\s*([^\n]+)', text)
    assert sans == ['DNS:'+name], sans
    print(f'PASS: OpenSSL verifies CSR signature and exact CN/single DNS SAN ({len(name)} bytes)')
elif sys.argv[2] == 'issue':
    name = (root/'hostname.txt').read_text()
    common = 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n'
    (root/'leaf.ext').write_text(common+'subjectAltName=DNS:'+name+'\n')
    (root/'wrong.ext').write_text(common+'subjectAltName=DNS:wrong.tempo.test\n')
    sign('leaf', 'intermediate', 'device.csr', 'leaf.ext')
    (root/'fullchain.pem').write_bytes((root/'leaf.pem').read_bytes()+(root/'intermediate.pem').read_bytes())
    request('wrong-key')
    for output, issuer, csr, ext, days in [
        ('wrong-host','intermediate','device.csr','wrong.ext','3'),
        ('wrong-key','intermediate','wrong-key.csr','leaf.ext','3'),
        ('untrusted','rogue','device.csr','leaf.ext','3'),
        ('expired','intermediate','device.csr','leaf.ext','3'),
        ('replacement','intermediate','device.csr','leaf.ext','4'),
    ]:
        if output == 'expired':
            (root/'ca-index.txt').write_text('')
            (root/'ca-serial.txt').write_text('1000\n')
            (root/'ca.conf').write_text('[ca]\ndefault_ca=issuer\n[issuer]\ndatabase=ca-index.txt\nserial=ca-serial.txt\nnew_certs_dir=.\ncertificate=intermediate.pem\nprivate_key=intermediate.key\ndefault_md=sha256\npolicy=names\n[names]\ncommonName=supplied\n')
            run('ca','-batch','-notext','-config','ca.conf','-in',csr,'-out',output+'.pem',
                '-extfile',ext,'-startdate','20200101000000Z','-enddate','20200102000000Z')
        else:
            sign(output,issuer,csr,ext,days)
        bundle = (root/(output+'.pem')).read_bytes()+(root/(issuer+'.pem')).read_bytes()
        (root/(output+'.pem')).write_bytes(bundle)
    (root/'malformed.pem').write_text('not a certificate')
else:
    raise ValueError('Unknown fixture phase')
