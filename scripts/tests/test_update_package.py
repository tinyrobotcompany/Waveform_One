import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('package_update',Path(__file__).parents[1]/'package-update.py')
package=importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class UpdatePackageTests(unittest.TestCase):
    def test_clean_download_directory_requires_all_artifacts_from_same_build(self):
        with tempfile.TemporaryDirectory() as directory:
            dist=Path(directory)
            (dist/'esp-update-build.json').write_text(json.dumps({'commit':'a'*40,'esp_elf_sha256':'b'*64}))
            for name in package.NAMES.values():(dist/name).write_bytes(b'validated-build')
            result=package.manifest(dist,'v1.0.0',1,'a'*40,'Release notes')
            self.assertEqual(result['source_ref'],'refs/heads/main')
            self.assertEqual(set(result['assets']),{'pi','esp','usb'})
            with self.assertRaises(ValueError):package.manifest(dist,'v1.0.0',1,'c'*40,'notes')
            (dist/package.NAMES['esp']).unlink()
            with self.assertRaises(FileNotFoundError):package.manifest(dist,'v1.0.0',1,'a'*40,'notes')
