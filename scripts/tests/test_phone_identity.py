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


if __name__=='__main__':unittest.main()
