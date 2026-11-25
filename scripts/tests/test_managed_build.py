import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('production', ROOT/'scripts/build-production.py')
production=importlib.util.module_from_spec(spec)
spec.loader.exec_module(production)

class ManagedBuild(unittest.TestCase):
    def test_accepts_shared_https_endpoint(self):
        production.validate_enrollment_url('https://service.example.com/v1/device')

    def test_rejects_credentials_redirect_paths_and_plaintext(self):
        for url in ('http://service.example/v1/device', 'https://user@service.example/v1/device',
                    'https://service.example/v1/device?x=1', 'https://service.example/other',
                    'https://service.example/v1/device\n'):
            with self.subTest(url=url), self.assertRaises(ValueError):
                production.validate_enrollment_url(url)
