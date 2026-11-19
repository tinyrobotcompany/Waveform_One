import json
from pathlib import Path
import tempfile
import unittest
from evidence import offline_plan,verify_offline,lookup_usage,recovery_manifest
class EvidenceTests(unittest.TestCase):
 def test_not_run_cannot_pass(self):
  value=offline_plan('a'*40)
  with self.assertRaises(ValueError):verify_offline(value,Path('/tmp'))
 def test_attestation_requires_every_file(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'proof.txt').write_text('synthetic fixture, not hardware evidence')
   value=offline_plan('a'*40);value.update(hardware='fixture',operator='test',tested_utc='2026-11-19')
   for item in value['cases']:item.update(status='pass',duration_s=1,evidence='proof.txt')
   self.assertTrue(verify_offline(value,root)['operator_attested'])
   value['cases'][0]['evidence']='../missing'
   with self.assertRaises(ValueError):verify_offline(value,root)
 def test_usage_redacts_metadata(self):
  with tempfile.TemporaryDirectory() as d:
   path=Path(d)/'log';path.write_text('Lookup id=1 audio_ms=1000\nLookup id=1 result=matched title="private"\nLookup id=2 audio_ms=1000\n')
   result=lookup_usage(path);self.assertEqual(result['logged_identification_attempts'],2);self.assertNotIn('private',json.dumps(result));self.assertIsNone(result['provider_billed_units'])
 def test_recovery_rejects_embedded_credentials(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);folder=root/'esp-idf/main';folder.mkdir(parents=True)
   (folder/'acrcloud_config.h').write_text('#define ACRCLOUD_HOST "not-empty"\n')
   with self.assertRaises(ValueError):recovery_manifest(root)
if __name__=='__main__':unittest.main()
