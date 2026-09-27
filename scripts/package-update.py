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
    pi_build=json.loads((dist/'pi-update-build.json').read_text())
    if pi_build['commit']!=commit:raise ValueError('Pi artifact belongs to a different commit')
    result=dict(schema=1,product='waveform-one',hardware='esp32s3-16mb',
                platform='linux-aarch64',python='3.13',protocol=1,channel='stable',source_ref='refs/heads/main',
                sequence=sequence,version=version,commit=commit,notes=notes,
                esp_elf_sha256=build['esp_elf_sha256'],assets={})
    if build['commit']!=commit:raise ValueError('ESP artifact belongs to a different commit')
    for key,name in NAMES.items():
        data=(dist/name).read_bytes()
        digest=hashlib.sha256(data).hexdigest()
        provenance=pi_build if key=='pi' else build
        if provenance.get('sha256',{}).get(name)!=digest:
            raise ValueError(f'Artifact does not match its validated build: {name}')
        result['assets'][key]=dict(name=name,size=len(data),sha256=digest)
    return validate(result,0)

def release_sequence(run_number, offset=''):
    # Keep the offset across runs after a workflow/repository counter migration.
    run=int(run_number);base=int(offset or '0')
    if run<1 or base<0 or run+base>=2**53:
        raise ValueError('Invalid release sequence or migration offset')
    return run+base

def record_pi(dist,commit):
    name=NAMES['pi']
    (dist/'pi-update-build.json').write_text(json.dumps({'commit':commit,
        'sha256':{name:hashlib.sha256((dist/name).read_bytes()).hexdigest()}}))


if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--record-pi':
        record_pi(Path(sys.argv[2]),os.environ['GITHUB_SHA'])
        sys.exit(0)
    if len(sys.argv)==3 and sys.argv[1]=='--verify':
        manifest(Path(sys.argv[2]),'v0.0.0',1,os.environ['GITHUB_SHA'],'Pre-sign verification')
        print('Verified all Pi/ESP assets against build hashes and GITHUB_SHA')
        sys.exit(0)

    p=argparse.ArgumentParser();p.add_argument('dist',type=Path);p.add_argument('plan',type=Path)
    args=p.parse_args();plan=json.loads(args.plan.read_text())
    result=manifest(args.dist,plan['tag'],release_sequence(os.environ['GITHUB_RUN_NUMBER'],os.environ.get('UPDATE_SEQUENCE_OFFSET','')),os.environ['GITHUB_SHA'],plan['notes'])
    (args.dist/'update.json').write_text(json.dumps(result,sort_keys=True,separators=(',',':'))+'\n')
    key=os.environ.get('UPDATE_SIGNING_KEY','')
    if not key:raise RuntimeError('UPDATE_SIGNING_KEY must be configured before publishing updates')
    with tempfile.TemporaryDirectory() as temp:
        private=Path(temp)/'signing.pem';private.write_text(key);private.chmod(0o600)
        subprocess.run(['openssl','dgst','-sha256','-sign',str(private),'-out',str(args.dist/'update.sig'),str(args.dist/'update.json')],check=True)

    verify_signature((args.dist/'update.json').read_bytes(),(args.dist/'update.sig').read_bytes(),
                     Path(__file__).resolve().parents[1]/'pi/updater/release-public.pem')
