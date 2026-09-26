import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location('package', Path(__file__).parents[1] / 'package-firmware.py')
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def test_bundle_preserves_flash_paths_and_records_checksums(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / 'build'
            (build / 'bootloader').mkdir(parents=True)
            (build / 'bootloader/bootloader.bin').write_bytes(b'boot')
            (build / 'app.bin').write_bytes(b'app')
            (build / 'flash_args').write_text('0x0 bootloader/bootloader.bin\n0x10000 app.bin\n')
            (build / 'flasher_args.json').write_text(json.dumps({
                'flash_files': {'0x0': 'bootloader/bootloader.bin', '0x10000': 'app.bin'},
                'extra_esptool_args': {'chip': 'esp32s3'}}))
            output = root / 'dist'
            package.bundle(build, output, 'abc123')
            archive = output / 'waveform-one-esp32s3-usb.zip'
            with zipfile.ZipFile(archive) as z:
                self.assertEqual(z.read('app.bin'), b'app')
                self.assertEqual(z.read('bootloader/bootloader.bin'), b'boot')
                manifest = json.loads(z.read('manifest.json'))
                self.assertEqual(manifest['commit'], 'abc123')
                self.assertFalse(manifest['otaReady'])
                self.assertEqual(manifest['sha256']['app.bin'], hashlib.sha256(b'app').hexdigest())
            self.assertIn(hashlib.sha256(archive.read_bytes()).hexdigest(),
                          (output / 'SHA256SUMS').read_text())

    def test_rejects_files_outside_build(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'flasher_args.json').write_text(json.dumps({
                'flash_files': {'0x0': '../secret'}, 'extra_esptool_args': {'chip': 'esp32s3'}}))
            with self.assertRaises(ValueError):
                package.bundle(root, root / 'out', 'abc')

    def test_ota_asset_and_build_identity_are_published_separately(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);build=root/'build';build.mkdir()
            image=bytearray(288);image[0]=0xe9
            image[32:36]=(0xabcd5432).to_bytes(4,'little')
            image[176:208]=hashlib.sha256(b'elf').digest()
            (build/'waveform_visualizer.bin').write_bytes(image)
            (build/'waveform_visualizer.elf').write_bytes(b'elf')
            (build/'flash_args').write_text('0x20000 waveform_visualizer.bin')
            (build/'flasher_args.json').write_text(json.dumps({
                'flash_files':{'0x20000':'waveform_visualizer.bin'},
                'extra_esptool_args':{'chip':'esp32s3'}}))
            package.bundle(build,root/'dist','a'*40)
            self.assertEqual((root/'dist/waveform-one-esp32s3-ota.bin').read_bytes(),image)
            metadata=json.loads((root/'dist/esp-update-build.json').read_text())
            self.assertEqual(metadata['esp_elf_sha256'],hashlib.sha256(b'elf').hexdigest())

    def test_rejects_image_with_different_embedded_elf_identity(self):
        image=bytearray(288);image[0]=0xe9
        image[32:36]=(0xabcd5432).to_bytes(4,'little')
        image[176:208]=hashlib.sha256(b'old elf').digest()
        with self.assertRaisesRegex(ValueError,'identity'):
            package.image_identity(image,b'new elf')
        image[176:208]=hashlib.sha256(b'new elf').digest()
        self.assertEqual(package.image_identity(image,b'new elf'),image[176:208].hex())
