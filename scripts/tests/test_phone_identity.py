"""Real OpenSSL certificates: never replace verification with a mocked success."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('identity', ROOT/'scripts/phone-identity.py')
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)


class IdentityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temporary.name)/'device'
        identity.create(cls.directory, 'device-a7f3.waveform.tinyrobotcompany.io')
        identity.development_certificate(cls.directory)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def copy(self, root):
        target=Path(root)/'identity'
        shutil.copytree(self.directory,target)
        target.chmod(0o700)
        return target

    def test_real_matching_identity_and_chain(self):
        self.assertEqual(identity.validate(self.directory,self.directory/'ca.pem'),'device-a7f3.waveform.tinyrobotcompany.io')
        self.assertEqual((self.directory/'device.key').stat().st_mode & 0o777,0o600)
        self.assertNotEqual((self.directory/'device.key').read_bytes(),(self.directory/'ca.key').read_bytes())
        with self.assertRaises(ValueError):identity.create(self.directory,'other.example.com')
        with self.assertRaises(ValueError):identity.development_certificate(self.directory)

    def test_wrong_hostname_key_untrusted_and_expiring_certificate(self):
        with tempfile.TemporaryDirectory() as root:
            target=self.copy(root)
            (target/'identity.json').write_text('{"hostname":"other.waveform.tinyrobotcompany.io"}')
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')
            (target/'identity.json').write_bytes((self.directory/'identity.json').read_bytes())
            other=Path(root)/'other'
            identity.create(other,'other.waveform.tinyrobotcompany.io')
            identity.development_certificate(other)
            with self.assertRaises(ValueError):identity.validate(target,other/'ca.pem')
            (target/'device.key').write_bytes((other/'device.key').read_bytes())
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')
            (target/'device.key').write_bytes((self.directory/'device.key').read_bytes())
            identity.run('openssl','x509','-req','-in',str(target/'device.csr'),'-CA',str(target/'ca.pem'),
                         '-CAkey',str(target/'ca.key'),'-CAcreateserial','-days','1',
                         '-extfile',str(target/'extensions.cnf'),'-out',str(target/'fullchain.pem'))
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')

    def test_bounds_permissions_and_names(self):
        for name in ['','localhost','192.168.1.2','*.example.com','a..example.com','-a.example.com','a-.example.com','UPPER.example.com','a'*64+'.com','a.com/other','a'*97]:
            with self.assertRaises(ValueError):identity.hostname(name)
        with tempfile.TemporaryDirectory() as root:
            target=self.copy(root)
            target.chmod(0o755)
            with self.assertRaises(ValueError):identity.private_directory(target)
            target.chmod(0o700)
            (target/'device.key').chmod(0o644)
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')
            (target/'device.key').chmod(0o600)
            (target/'fullchain.pem').write_text('x'*8192)
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')

    def test_self_signed_leaf_cannot_be_its_own_trust_anchor(self):
        with tempfile.TemporaryDirectory() as root:
            target=self.copy(root)
            identity.run('openssl','req','-x509','-new','-key',str(target/'device.key'),
                '-subj','/CN=device-a7f3.waveform.tinyrobotcompany.io','-days','30',
                '-addext','subjectAltName=DNS:device-a7f3.waveform.tinyrobotcompany.io',
                '-addext','basicConstraints=critical,CA:FALSE','-out',str(target/'fullchain.pem'))
            with self.assertRaisesRegex(ValueError,'separate CA'):
                identity.validate(target,target/'fullchain.pem')

    def test_intermediate_chain_and_extra_pem_rejection(self):
        with tempfile.TemporaryDirectory() as root:
            target=self.copy(root)
            identity.run('openssl','req','-x509','-new','-key',str(target/'ca.key'),
                '-subj','/CN=Root','-days','365','-addext','basicConstraints=critical,CA:TRUE,pathlen:1',
                '-addext','keyUsage=critical,keyCertSign,cRLSign','-out',str(target/'ca.pem'))
            intermediate=Path(root)/'intermediate'
            identity.create(intermediate,'issuer.example.com')
            ext=intermediate/'ca.cnf'
            ext.write_text('basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\n')
            identity.run('openssl','x509','-req','-in',str(intermediate/'device.csr'),'-CA',str(target/'ca.pem'),
                '-CAkey',str(target/'ca.key'),'-CAcreateserial','-days','90','-extfile',str(ext),'-out',str(intermediate/'issuer.pem'))
            identity.run('openssl','x509','-req','-in',str(target/'device.csr'),'-CA',str(intermediate/'issuer.pem'),
                '-CAkey',str(intermediate/'device.key'),'-CAcreateserial','-days','30','-extfile',str(target/'extensions.cnf'),'-out',str(target/'fullchain.pem'))
            leaf=(target/'fullchain.pem').read_bytes()
            chain=leaf+(intermediate/'issuer.pem').read_bytes()
            (target/'fullchain.pem').write_bytes(chain)
            self.assertEqual(identity.validate(target,target/'ca.pem'),'device-a7f3.waveform.tinyrobotcompany.io')
            (target/'fullchain.pem').write_bytes(leaf)
            with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')
            for extra in (b'garbage', (target/'device.key').read_bytes(), b'-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----'):
                (target/'fullchain.pem').write_bytes(chain+extra)
                with self.assertRaises(ValueError):identity.validate(target,target/'ca.pem')


if __name__=='__main__':unittest.main()
