import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import updates

class UpdatesTests(unittest.TestCase):
    def manifest(self):
        return dict(schema=1, product='waveform-one', hardware='esp32s3-16mb',
                    platform='linux-aarch64', python='3.13', protocol=1,
                    channel='stable', source_ref='refs/heads/main', sequence=2, version='v0.2.0', notes='Update',
                    commit='a'*40, esp_elf_sha256='b'*64,
                    assets={k:dict(name=n,size=3,sha256=hashlib.sha256(b'abc').hexdigest()) for k,n in
                            [('pi','waveform-one-pi.tar.gz'),('esp','waveform-one-esp32s3-ota.bin'),('usb','waveform-one-esp32s3-usb.zip')]})

    def test_compatibility_and_monotonic_versions(self):
        m=self.manifest()
        updates.validate(m, 1)
        for key,value in [('source_ref','refs/heads/feature'),('hardware','unknown'),('protocol',2),('sequence',1),('platform','x86_64'),('python','3.12')]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                updates.validate(dict(m,**{key:value}),1)
        m['assets']['pi']['name']='../escape'
        with self.assertRaises(ValueError): updates.validate(m,1)

    def test_signed_bytes_cannot_be_modified(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d); key=d/'key'; pub=d/'pub'; data=d/'data'; sig=d/'sig'
            subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(key)],check=True,capture_output=True)
            subprocess.run(['openssl','pkey','-in',str(key),'-pubout','-out',str(pub)],check=True,capture_output=True)
            data.write_bytes(json.dumps(self.manifest()).encode())
            subprocess.run(['openssl','dgst','-sha256','-sign',str(key),'-out',str(sig),str(data)],check=True)
            updates.verify_signature(data.read_bytes(),sig.read_bytes(),pub)
            with self.assertRaises(ValueError): updates.verify_signature(data.read_bytes()+b' ',sig.read_bytes(),pub)

    def test_archive_rejects_links_and_traversal_before_extraction(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d)
            for name,link in [('../escape',False),('/absolute',False),('link',True)]:
                archive=d/'a.tar.gz'
                with tarfile.open(archive,'w:gz') as t:
                    i=tarfile.TarInfo(name);i.size=1
                    if link: i.type=tarfile.SYMTYPE;i.linkname='/tmp/out'
                    t.addfile(i,io.BytesIO(b'x'))
                with self.assertRaises(ValueError): updates.extract(archive,d/'out')
            self.assertFalse((d/'out').exists())

    def test_download_hash_and_size(self):
        asset=self.manifest()['assets']['pi']
        updates.verify_asset(b'abc',asset)
        for value in [b'ab',b'abcd',b'xyz']:
            with self.assertRaises(ValueError):updates.verify_asset(value,asset)

if __name__=='__main__':unittest.main()
