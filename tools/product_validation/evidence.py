"""Prepare inspectable offline/recovery evidence without touching a connected device."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

CASES = ('cold_boot_no_wifi','cold_boot_no_credentials','internet_blocked_lan_available',
         'provider_unreachable','long_playback_offline','silence_resume_offline','connectivity_restored')


def offline_plan(firmware_sha):
    if not re.fullmatch('[0-9a-f]{40}',firmware_sha):raise ValueError('Exact commit required')
    return {'schema':1,'firmware_sha':firmware_sha,'hardware':None,'operator':None,'tested_utc':None,
            'cases':[dict(case=case,status='not_run',duration_s=None,evidence=None,notes='') for case in CASES]}


def verify_offline(value, root):
    if set(item['case'] for item in value.get('cases',[])) != set(CASES) or len(value['cases'])!=len(CASES):raise ValueError('Incomplete case inventory')
    for field in ('hardware','operator','tested_utc'):
        if not value.get(field):raise ValueError('Missing '+field)
    offline_plan(value.get('firmware_sha',''))
    evidence=[]
    for item in value['cases']:
        if item['status']!='pass':raise ValueError('Offline acceptance incomplete: '+item['case'])
        if isinstance(item['duration_s'],bool) or not isinstance(item['duration_s'],(int,float)) or not math.isfinite(item['duration_s']) or item['duration_s']<=0:raise ValueError('Measured duration required')
        path=(root/str(item['evidence'])).resolve()
        if not path.is_relative_to(root.resolve()) or not path.is_file() or path.stat().st_size==0:raise ValueError('Missing/local evidence required')
        evidence.append(dict(case=item['case'],sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    return {'firmware_sha':value['firmware_sha'],'operator_attested':True,'evidence':evidence,
            'limitation':'Validates completeness/file integrity, not physical behavior or truth of attestations.'}


def lookup_usage(path):
    # Start markers count attempts, including errors; never equate these with billing.
    pattern=re.compile(r'Lookup id=(\d+) audio_ms=')
    count=0
    with path.open(errors='replace') as source:
        for line in source:
            if pattern.search(line):count+=1
    return {'logged_identification_attempts':count,'provider_billed_units':None,
            'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'limitation':'Requires complete logs for the measured interval; no rates without measured listening duration. No titles/audio emitted.'}


def recovery_manifest(build):
    config=build/'esp-idf/main/acrcloud_config.h'
    if not config.is_file():raise ValueError('Missing generated credential configuration')
    text=config.read_text()
    for name in ('HOST','ACCESS_KEY','ACCESS_SECRET'):
        if not re.search(r'^#define ACRCLOUD_'+name+r' ""$',text,re.M):raise ValueError('Refusing manifest: build may embed credentials')
    args=json.loads((build/'flasher_args.json').read_text())
    files=args.get('flash_files',{})
    if not files:raise ValueError('Missing IDF flash file inventory')
    artifacts=[]
    for address,relative in files.items():
        path=(build/relative).resolve()
        if not path.is_file():raise ValueError('Missing flash artifact')
        # IDF model may live outside build; list hashes, never copy private trees.
        artifacts.append(dict(address=address,name=path.name,bytes=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    return {'schema':1,'chip':args.get('extra_esptool_args',{}).get('chip'),
            'embedded_acrcloud_fields_empty':True,'flash_artifacts':artifacts,
            'limitation':'Inventory only, not a hardware test or a guarantee all binary content is secret-free. Review the build source and configuration.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    plan=sub.add_parser('offline-plan');plan.add_argument('firmware_sha')
    verify=sub.add_parser('verify-offline');verify.add_argument('manifest',type=Path)
    usage=sub.add_parser('lookup-usage');usage.add_argument('log',type=Path)
    recovery=sub.add_parser('recovery-manifest');recovery.add_argument('build',type=Path)
    args=parser.parse_args()
    if args.command=='offline-plan':value=offline_plan(args.firmware_sha)
    elif args.command=='verify-offline':value=verify_offline(json.loads(args.manifest.read_text()),args.manifest.parent)
    elif args.command=='lookup-usage':value=lookup_usage(args.log)
    else:value=recovery_manifest(args.build)
    print(json.dumps(value,indent=2,allow_nan=False))
if __name__=='__main__':main()
