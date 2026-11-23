import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).parents[1] / 'verify-release-assets.py'
spec = importlib.util.spec_from_file_location('release_assets', SCRIPT)
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class ReleaseAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dist = self.root / 'dist'
        self.dist.mkdir()
        for name in assets.RELEASE_ASSETS:
            (self.dist / name).write_bytes(('validated ' + name).encode())
        self.downloaded = self.root / 'downloaded'
        shutil.copytree(self.dist, self.downloaded)

    def test_p4_release_matches_preflight_and_post_upload_cli(self):
        subprocess.run([sys.executable, str(SCRIPT), '--dist', str(self.dist)], check=True)
        subprocess.run([sys.executable, str(SCRIPT), '--dist', str(self.dist),
                        '--downloaded', str(self.downloaded)], check=True)

    def test_every_p4_asset_is_required_and_hash_checked(self):
        for name in sorted(assets.RELEASE_ASSETS):
            with self.subTest(asset=name):
                target = self.downloaded / name
                original = target.read_bytes()
                target.write_bytes(b'corrupted upload')
                with self.assertRaises(ValueError): assets.verify_assets(self.dist, self.downloaded)
                target.unlink()
                with self.assertRaises(ValueError): assets.verify_assets(self.dist, self.downloaded)
                target.write_bytes(original)

    def test_unexpected_hidden_files_directories_and_links_fail_closed(self):
        for directory in [self.dist, self.downloaded]:
            for extra in ['unpublished.json', '.hidden-build-state']:
                with self.subTest(directory=directory.name, extra=extra):
                    (directory / extra).write_bytes(b'extra')
                    with self.assertRaises(ValueError): assets.verify_assets(self.dist, self.downloaded)
                    (directory / extra).unlink()
            path = directory / 'waveform-p4.bin'
            original = path.read_bytes()
            path.unlink()
            path.mkdir()
            with self.assertRaises(ValueError): assets.verify_assets(self.dist, self.downloaded)
            path.rmdir()
            outside = self.root / 'outside.bin'
            outside.write_bytes(original)
            path.symlink_to(outside)
            with self.assertRaises(ValueError): assets.verify_assets(self.dist, self.downloaded)
            path.unlink()
            path.write_bytes(original)

    def test_workflow_validates_before_draft_mutation_and_after_download(self):
        workflow = (SCRIPT.parents[1] / '.github/workflows/release.yml').read_text()
        preflight = workflow.index('python3 scripts/verify-release-assets.py --dist "$RELEASE_DIST"')
        create = workflow.index('gh release create')
        delete = workflow.index('gh release delete-asset')
        download = workflow.index('gh release download')
        verify = workflow.index('python3 scripts/verify-release-assets.py --dist "$RELEASE_DIST" --downloaded')
        publish = workflow.index('gh release edit')
        self.assertLess(preflight, create)
        self.assertLess(preflight, delete)
        self.assertLess(download, verify)
        self.assertLess(verify, publish)

    def test_release_contains_only_p4_assets(self):
        self.assertEqual(assets.RELEASE_ASSETS, assets.P4_ASSETS)
        (self.dist / 'waveform-one-pi.tar.gz').write_bytes(b'retired')
        with self.assertRaises(ValueError): assets.validate_staging(self.dist)
