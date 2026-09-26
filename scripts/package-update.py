#!/usr/bin/env python3
"""Create the immutable signed update manifest after validation, in release CI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'pi/updater'))
from updates import NAMES,validate,verify_signature


def manifest(dist,version,sequence,commit,notes):
    build=json.loads((dist/'esp-update-build.json').read_text())
    result=dict(schema=1,product='waveform-one',hardware='esp32s3-16mb',
                platform='linux-aarch64',python='3.13',protocol=1,channel='stable',source_ref='refs/heads/main',
                sequence=sequence,version=version,commit=commit,notes=notes,
                esp_elf_sha256=build['esp_elf_sha256'],assets={})
    if build['commit']!=commit:raise ValueError('ESP artifact belongs to a different commit')
    for key,name in NAMES.items():
        data=(dist/name).read_bytes()
        result['assets'][key]=dict(name=name,size=len(data),sha256=hashlib.sha256(data).hexdigest())
    return validate(result,0)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('dist',type=Path);p.add_argument('plan',type=Path)
    args=p.parse_args();plan=json.loads(args.plan.read_text())
    result=manifest(args.dist,plan['tag'],int(os.environ['GITHUB_RUN_NUMBER']),os.environ['GITHUB_SHA'],plan['notes'])
    (args.dist/'update.json').write_text(json.dumps(result,sort_keys=True,separators=(',',':'))+'\n')
    key=os.environ.get('UPDATE_SIGNING_KEY','')
    if not key:raise RuntimeError('UPDATE_SIGNING_KEY must be configured before publishing updates')
    with tempfile.TemporaryDirectory() as temp:
        private=Path(temp)/'signing.pem';private.write_text(key);private.chmod(0o600)
        subprocess.run(['openssl','dgst','-sha256','-sign',str(private),'-out',str(args.dist/'update.sig'),str(args.dist/'update.json')],check=True)

    verify_signature((args.dist/'update.json').read_bytes(),(args.dist/'update.sig').read_bytes(),
                     Path(__file__).resolve().parents[1]/'pi/updater/release-public.pem')
