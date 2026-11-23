import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('production',ROOT/'scripts/build-production.py')
production=importlib.util.module_from_spec(spec);spec.loader.exec_module(production)
class ProductionSecurity(unittest.TestCase):
    def test_every_protection_is_mandatory(self):
        good='\n'.join('CONFIG_'+n+'=y' for n in production.REQUIRED)+'\nCONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.production.csv"'
        production.verify_config(good)
        for name in production.REQUIRED:
            with self.assertRaises(ValueError):production.verify_config(good.replace('CONFIG_'+name+'=y',''))
        for name in production.FORBIDDEN:
            with self.assertRaises(ValueError):production.verify_config(good+'\nCONFIG_'+name+'=y')
    def test_factory_layout_has_no_overlap_and_key_storage_is_encrypted(self):
        entries=[]
        for line in (production.APP/'partitions.production.csv').read_text().splitlines():
            if not line or line.startswith('#'):continue
            name,kind,sub,offset,size,flags=map(str.strip,line.split(','))
            size=int(size[:-1])*1024*1024 if size.endswith('M') else int(size,0)
            entries.append((int(offset,0),size))
            if name=='nvs_keys':self.assertEqual((sub,size,flags),('nvs_keys',4096,'encrypted'))
        entries.sort()
        self.assertGreaterEqual(entries[0][0],0x11000)
        for (a,n),(b,_) in zip(entries,entries[1:]):self.assertLessEqual(a+n,b)
        self.assertLessEqual(sum(entries[-1]),0x2000000)

    def test_secure_ota_requires_boot_signature_verification(self):
        from unittest.mock import patch
        spec=importlib.util.spec_from_file_location('p4',ROOT/'scripts/package-p4-update.py')
        p4=importlib.util.module_from_spec(spec);spec.loader.exec_module(p4)
        with self.assertRaises(ValueError): p4.verify_secure_image(Path('firmware.bin'), None)
        with patch.object(p4.subprocess,'run') as run:
            p4.verify_secure_image(Path('firmware.bin'),Path('public.pem'))
            args=run.call_args.args[0]
            self.assertIn('verify-signature',args)
            self.assertIn('public.pem',args)
