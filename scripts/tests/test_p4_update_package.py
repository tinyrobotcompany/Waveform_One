import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('p4_package', Path(__file__).parents[1] / 'package-p4-update.py')
p4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p4)
release_spec = importlib.util.spec_from_file_location('release_assets', Path(__file__).parents[1] / 'verify-release-assets.py')
release_assets = importlib.util.module_from_spec(release_spec)
release_spec.loader.exec_module(release_assets)


def image(version='1.2.3'):
    blob = bytearray(1024)
    blob[0] = 0xe9
    blob[32:36] = (0xabcd5432).to_bytes(4, 'little')
    blob[48:48+len(version)] = version.encode()
    name = b'waveform_p4_display'
    blob[80:80+len(name)] = name
    return blob


class P4PackageTests(unittest.TestCase):
    def test_ci_signing_command_creates_a_verifiable_manifest_and_cleans_up_key(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Run the actual CLI against an isolated publisher public key, never the real key.
            script = root / 'scripts/package-p4-update.py'
            script.parent.mkdir()
            shutil.copyfile(Path(p4.__file__), script)
            public = root / 'firmware/esp32/p4_display/main/ota_public.pem'
            public.parent.mkdir(parents=True)
            private = root / 'fixture.pem'
            subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt',
                            'rsa_keygen_bits:2048', '-out', str(private)], check=True, capture_output=True)
            subprocess.run(['openssl', 'pkey', '-in', str(private), '-pubout', '-out', str(public)],
                           check=True, capture_output=True)
            dist = root / 'dist'; dist.mkdir()
            (dist / p4.IMAGE).write_bytes(image())
            (dist / 'p4-dependencies.lock').write_text('pinned dependencies')
            (dist / 'p4-update-build.json').write_text(json.dumps(dict(
                commit='a'*40, version='1.2.3', sha256=p4.digest(dist / p4.IMAGE),
                dependency_lock_sha256=p4.digest(dist / 'p4-dependencies.lock'))))
            plan = root / 'plan.json'
            plan.write_text(json.dumps(dict(tag='v1.2.3', commit='a'*40, notes='CLI regression test')))
            scratch = root / 'scratch'; scratch.mkdir()
            environment = dict(os.environ, GITHUB_SHA='a'*40, UPDATE_SIGNING_KEY=private.read_text(),
                               TMPDIR=str(scratch), TEMP=str(scratch), TMP=str(scratch))
            result = subprocess.run([sys.executable, str(script), 'package', '--dist', str(dist),
                                     '--plan', str(plan)], env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn(private.read_text(), result.stdout + result.stderr)
            self.assertEqual(list(scratch.iterdir()), [])
            self.assertEqual({path.name for path in dist.iterdir()}, release_assets.P4_ASSETS)
            subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(public), '-signature',
                            str(dist / 'waveform-p4-manifest.sig'), str(dist / 'waveform-p4-manifest.json')],
                           check=True, capture_output=True)

    def test_cli_private_key_is_owner_only_and_removed_on_signing_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan = root / 'plan.json'; plan.write_text('{}')
            observed = []

            def fail_signing(dist, value, commit, private, public):
                observed.append(private)
                self.assertEqual(stat.S_IMODE(private.stat().st_mode), 0o600)
                self.assertEqual(private.read_text(), 'test-only key content')
                raise RuntimeError('Simulated signing failure')

            with patch.object(sys, 'argv', ['package-p4-update.py', 'package', '--dist', str(root),
                                            '--plan', str(plan)]), \
                    patch.dict(os.environ, {'GITHUB_SHA': 'a'*40, 'UPDATE_SIGNING_KEY': 'test-only key content'}), \
                    patch.object(p4, 'package', side_effect=fail_signing):
                with self.assertRaisesRegex(RuntimeError, 'Simulated signing failure'):
                    p4.main()
            self.assertEqual(len(observed), 1)
            self.assertFalse(observed[0].exists())
            self.assertFalse(observed[0].parent.exists())

    def test_record_requires_32bit_mapping_in_application_and_bootloader(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / 'build'
            headers = [build / 'config/sdkconfig.h', build / 'bootloader/config/sdkconfig.h']
            rollback = '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n'
            addressing = '#define CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH 1\n'
            for header in headers:
                header.parent.mkdir(parents=True)
                header.write_text(rollback + addressing)
            (build / 'waveform_p4_display.bin').write_bytes(image())
            for header in headers:
                with self.subTest(header=header):
                    header.write_text(rollback)
                    with self.assertRaisesRegex(ValueError, '32-bit flash addressing'):
                        p4.record(build, root / 'dist', '1.2.3', 'a'*40)
                    self.assertFalse((root / 'dist').exists())
                    header.write_text(rollback + addressing)
            p4.record(build, root / 'dist', '1.2.3', 'a'*40)
            self.assertEqual((root / 'dist' / p4.IMAGE).read_bytes(), image())

    def test_image_identity_version_and_slot_are_required(self):
        p4.validate_image(image(), '1.2.3')
        for blob, version in [(image(), '1.2.4'), (image(), '01.2.3'),
                              (bytes(1024), '1.2.3'), (image() + bytes(0x800000), '1.2.3'),
                              (image() + b'-----BEGIN PRIVATE KEY-----\n' + b'A'*64 + b'\n-----END PRIVATE KEY-----', '1.2.3')]:
            with self.subTest(version=version), self.assertRaises(ValueError):
                p4.validate_image(blob, version)

    def test_signed_manifest_and_draft_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dist = root / 'dist'; dist.mkdir()
            (dist / p4.IMAGE).write_bytes(image())
            (dist / 'p4-dependencies.lock').write_text('pinned dependencies')
            provenance = dict(commit='a'*40, version='1.2.3', sha256=p4.digest(dist / p4.IMAGE),
                              dependency_lock_sha256=p4.digest(dist / 'p4-dependencies.lock'))
            (dist / 'p4-update-build.json').write_text(json.dumps(provenance))
            plan = dict(tag='v1.2.3', commit='a'*40, notes='What changed')
            private, public = root / 'key.pem', root / 'public.pem'
            subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', str(private)], check=True, capture_output=True)
            subprocess.run(['openssl', 'pkey', '-in', str(private), '-pubout', '-out', str(public)], check=True, capture_output=True)
            p4.package(dist, plan, 'a'*40, private, public)
            manifest = json.loads((dist / 'waveform-p4-manifest.json').read_text())
            self.assertEqual(manifest['target'], p4.TARGET)
            self.assertEqual(manifest['layout'], p4.LAYOUT)
            self.assertEqual(manifest['version'], '1.2.3')
            self.assertTrue(manifest['url'].endswith('/v1.2.3/waveform-p4.bin'))
            # The workflow combines P4 output and legacy assets into a single flat release.
            self.assertEqual({p.name for p in dist.iterdir()}, release_assets.P4_ASSETS)
            for name in release_assets.LEGACY_ASSETS:
                (dist / name).write_bytes(('legacy fixture: ' + name).encode())
            downloaded = root / 'downloaded'
            shutil.copytree(dist, downloaded)
            release_assets.verify_assets(dist, downloaded)
            (downloaded / p4.IMAGE).write_bytes(b'corrupt upload')
            with self.assertRaises(ValueError): release_assets.verify_assets(dist, downloaded)
            shutil.copyfile(dist / p4.IMAGE, downloaded / p4.IMAGE)
            (downloaded / 'stale.bin').write_bytes(b'leftover draft')
            with self.assertRaises(ValueError): release_assets.verify_assets(dist, downloaded)
            with self.assertRaises(ValueError): p4.package(dist, plan, 'b'*40, private, public)
            (dist / p4.IMAGE).write_bytes(image('1.2.4'))
            with self.assertRaises(ValueError): p4.package(dist, plan, 'a'*40, private, public)

    def test_layout_preserves_existing_data_and_fits_flash(self):
        entries = {}
        for line in (p4.APP / 'partitions.csv').read_text().splitlines():
            if not line.strip() or line.startswith('#'): continue
            name, kind, subtype, offset, size, *_ = [p.strip() for p in line.split(',')]
            entries[name] = (int(offset, 0), int(size[:-1])*1024**2 if size.endswith('M') else int(size, 0))
        self.assertEqual(entries['nvs'], (0x9000, 0x6000))
        self.assertEqual(entries['music_model'], (0x810000, 4*1024**2))
        self.assertEqual(entries['storage'], (0xc10000, 3*1024**2))
        regions = sorted(entries.values())
        for (offset, size), (next_offset, _) in zip(regions, regions[1:]):
            self.assertLessEqual(offset + size, next_offset)
        self.assertLessEqual(sum(regions[-1]), 32*1024**2)
        for name in ['ota_0', 'ota_1']:
            self.assertEqual(entries[name][1], 8*1024**2)
            self.assertEqual(entries[name][0] % 0x10000, 0)
