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
            package.record_pi(dist,'a'*40)
            import hashlib
            metadata=json.loads((dist/'esp-update-build.json').read_text())
            metadata['sha256']={name:hashlib.sha256((dist/name).read_bytes()).hexdigest() for name in package.NAMES.values()}
            (dist/'esp-update-build.json').write_text(json.dumps(metadata))
            result=package.manifest(dist,'v1.0.0',1,'a'*40,'Release notes')
            self.assertEqual(result['source_ref'],'refs/heads/main')
            self.assertEqual(set(result['assets']),{'pi','esp','usb'})
            with self.assertRaises(ValueError):package.manifest(dist,'v1.0.0',1,'c'*40,'notes')
            (dist/package.NAMES['esp']).unlink()
            with self.assertRaises(FileNotFoundError):package.manifest(dist,'v1.0.0',1,'a'*40,'notes')

    def test_pi_provenance_is_required_and_bound_to_archive_bytes(self):
        import hashlib
        with tempfile.TemporaryDirectory() as directory:
            dist=Path(directory)
            for name in package.NAMES.values():(dist/name).write_bytes(b'validated-build')
            hashes={name:hashlib.sha256((dist/name).read_bytes()).hexdigest() for name in package.NAMES.values()}
            (dist/'esp-update-build.json').write_text(json.dumps({'commit':'a'*40,'esp_elf_sha256':'b'*64,'sha256':hashes}))
            with self.assertRaises(FileNotFoundError):package.manifest(dist,'v1.0.0',1,'a'*40,'notes')
            pi=dict(commit='c'*40,sha256={package.NAMES['pi']:hashes[package.NAMES['pi']]})
            (dist/'pi-update-build.json').write_text(json.dumps(pi))
            with self.assertRaises(ValueError):package.manifest(dist,'v1.0.0',1,'a'*40,'notes')
            pi['commit']='a'*40
            (dist/'pi-update-build.json').write_text(json.dumps(pi))
            package.manifest(dist,'v1.0.0',1,'a'*40,'notes')
            for component in ['pi','esp','usb']:
                path=dist/package.NAMES[component];original=path.read_bytes();path.write_bytes(b'stale artifact')
                with self.subTest(component=component),self.assertRaises(ValueError):
                    package.manifest(dist,'v1.0.0',1,'a'*40,'notes')
                path.write_bytes(original)

    def test_migration_offset_preserves_sequence_after_run_counter_restart(self):
        self.assertEqual(package.release_sequence('100',''),100)
        self.assertEqual(package.release_sequence('1','100'),101)
        self.assertGreater(package.release_sequence('2','100'),101)
        for run,offset in [('0','0'),('1','-1'),('1','oops'),('1',str(2**53))]:
            with self.subTest(run=run,offset=offset),self.assertRaises(ValueError):
                package.release_sequence(run,offset)
