#!/usr/bin/env python3
"""Build and verify a manufacturing-only P4 image. Never flashes or burns eFuses."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/esp32/p4_display'
REQUIRED = ('SECURE_BOOT', 'SECURE_BOOT_V2_ENABLED', 'SECURE_BOOT_V2_RSA_ENABLED',
            'SECURE_BOOT_BUILD_SIGNED_BINARIES', 'SECURE_FLASH_ENC_ENABLED',
            'SECURE_FLASH_ENCRYPTION_MODE_RELEASE', 'NVS_ENCRYPTION',
            'NVS_SEC_KEY_PROTECT_USING_FLASH_ENC', 'SECURE_ENABLE_SECURE_ROM_DL_MODE',
            'WAVEFORM_PRODUCTION_SECURITY', 'BOOTLOADER_APP_ROLLBACK_ENABLE')
FORBIDDEN = ('SECURE_BOOT_INSECURE', 'SECURE_BOOT_ALLOW_JTAG', 'SECURE_INSECURE_ALLOW_DL_MODE',
             'SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE', 'EFUSE_VIRTUAL',
             'SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT')

def verify_config(config):
    for name in REQUIRED:
        if f'CONFIG_{name}=y' not in config.splitlines():
            raise ValueError('Missing production protection: '+name)
    for name in FORBIDDEN:
        if f'CONFIG_{name}=y' in config.splitlines():
            raise ValueError('Forbidden production setting: '+name)
    if 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.production.csv"' not in config.splitlines():
        raise ValueError('Production partition layout required')

def build(output, key, version, idf):
    if not re.fullmatch(r'\d+\.\d+\.\d+',version): raise ValueError('Semantic version required')
    output=output.resolve();key=key.resolve()
    if key.stat().st_mode & 0o077: raise ValueError('Signing key must be private (0600)')
    if any(c in str(key) for c in '\n\r";'): raise ValueError('Unsafe key path')
    output.mkdir(mode=0o700,parents=True,exist_ok=False)
    signing=output/'signing.defaults'
    signing.write_text('CONFIG_SECURE_BOOT_SIGNING_KEY="'+str(key)+'"\n')
    env=dict(os.environ,WAVEFORM_DEVICE_ACRCLOUD='1',WAVEFORM_RELEASE_VERSION=version)
    subprocess.run([sys.executable,str(idf/'tools/idf.py'),'-C',str(APP),'-B',str(output/'build'),
        '-DSDKCONFIG='+str(output/'sdkconfig'),
        '-DSDKCONFIG_DEFAULTS='+str(APP/'sdkconfig.defaults')+';'+str(APP/'sdkconfig.defaults.production')+';'+str(signing),
        'build'],env=env,check=True)
    verify_config((output/'sdkconfig').read_text())
    for binary in (output/'build/bootloader/bootloader.bin',output/'build/waveform_p4_display.bin'):
        subprocess.run([sys.executable,'-m','espsecure','verify-signature','--version','2','--keyfile',str(key),str(binary)],check=True)
    print('Verified factory build. NOT flashed. First boot permanently enables hardware protections.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--signing-key',type=Path,required=True)
    p.add_argument('--version',required=True)
    p.add_argument('--idf',type=Path,default=Path(os.environ.get('IDF_PATH',Path.home()/'.espressif/v6.1/esp-idf')))
    a=p.parse_args()
    os.umask(0o077)
    build(a.output,a.signing_key,a.version,a.idf)
