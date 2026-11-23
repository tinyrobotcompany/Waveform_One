import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'firmware/esp32/p4_display'
class PhoneFirmware(unittest.TestCase):
    def test_tls_and_dns_boundaries(self):
        with tempfile.TemporaryDirectory() as tmp:
            for suite in ('tls', 'dns', 'tls_encrypted'):
                binary = str(Path(tmp)/suite)
                subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-D_POSIX_C_SOURCE=200112L', '-Wall', '-Wextra', '-Werror',
                    *(['-DCONFIG_NVS_ENCRYPTION=1'] if suite == 'tls_encrypted' else []),
                    '-I'+str(APP/'tests/tls_stubs'), '-I'+str(APP/'main'),
                    str(APP/'tests'/('phone_'+suite.replace('_encrypted', '')+'_test.c')), '-o', binary], check=True)
                subprocess.run([binary], check=True)
