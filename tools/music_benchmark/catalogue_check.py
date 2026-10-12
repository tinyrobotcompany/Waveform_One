"""Explicit, paced public-fixture service check; no audio playback or recording."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import time
import urllib.error
import urllib.request
import uuid

USER_AGENT = 'Dalvik/2.1.0 (Linux; U; Android 5.0.2; VS980 4G Build/LRX22G)'
LIMIT = 128 * 1024


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def lookup(signature):
    body = {'signature': {'uri': signature['uri'], 'samplems': signature['samplems']},
            'timezone': 'UTC', 'timestamp': int(time.time() * 1000), 'context': {}, 'geolocation': {}}
    url = 'https://amp.shazam.com/discovery/v5/en-US/GB/iphone/-/tag/' + str(uuid.uuid4()) + '/' + str(uuid.uuid4())
    url += '?sync=true&webv3=true&sampling=true&connected=&shazamapiversion=v3&sharehub=true&hubv5minorversion=v5.1&hidelb=true&video=v3'
    request = urllib.request.Request(url, data=json.dumps(body).encode(),
                                    headers={'Content-Type': 'application/json', 'User-Agent': USER_AGENT})
    opener = urllib.request.build_opener(NoRedirect)
    started = time.perf_counter()
    try:
        with opener.open(request, timeout=20) as response:
            data = response.read(LIMIT + 1)
            if len(data) > LIMIT:
                raise ValueError('Response exceeds device size limit')
            value = json.loads(data)
            track = value.get('track') or {}
            album = next((field.get('text', '') for section in track.get('sections', [])
                          for field in section.get('metadata', []) if field.get('title') == 'Album'), '')
            return {'http_status': response.status, 'lookup_ms': round((time.perf_counter()-started)*1000,3),
                    'matches': len(value.get('matches', [])), 'title': track.get('title',''),
                    'artist': track.get('subtitle',''), 'album': album, 'key': track.get('key',''),
                    'retry_after': response.headers.get('Retry-After',''), 'review': 'unreviewed'}
    except urllib.error.HTTPError as error:
        return {'http_status': error.code, 'lookup_ms': round((time.perf_counter()-started)*1000,3),
                'retry_after': error.headers.get('Retry-After',''), 'error': 'HTTP error; stop this run'}
    except (urllib.error.URLError, TimeoutError, ValueError) as error:
        return {'http_status': 0, 'lookup_ms': round((time.perf_counter()-started)*1000,3),
                'error': type(error).__name__ + ': ' + str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark-dir', required=True)
    parser.add_argument('--seconds', type=int, choices=(2,3,4,6,8), required=True)
    parser.add_argument('--offset', type=int, choices=(0,8,16), default=0)
    parser.add_argument('--cases', nargs='+', default=['jazz','classical','beyonce','wicked','rock'])
    parser.add_argument('--interval', type=float, default=60)
    args = parser.parse_args()
    if args.interval < 60 or len(args.cases) > 5 or len(set(args.cases)) != len(args.cases):
        parser.error('At most five distinct cases, at least sixty seconds between requests')
    root = Path(args.benchmark_dir).resolve()
    manifest = {item['id']: item for item in json.loads((root/'manifest.json').read_text())}
    # Only signatures from the checksum-verified benchmark's known public fixtures.
    plan = []
    for name in args.cases:
        if name not in manifest:
            parser.error('Unknown public fixture: '+name)
        item=manifest[name]
        if hashlib.sha256(Path(item['wav']).read_bytes()).hexdigest()!=item['wav_sha256']:
            raise ValueError('Public fixture checksum changed: '+name)
        signature=json.loads((root/'results/signatures'/('{}-{}-{}.json'.format(name,args.offset,args.seconds))).read_text())
        if not signature.get('accepted') or not signature['uri'].startswith('data:audio/vnd.shazam.sig;base64,'):
            raise ValueError('Invalid signature: '+name)
        plan.append((name,item,signature))
    records=[]; output=root/('catalogue-{}s-offset{}.json'.format(args.seconds,args.offset))
    for index,(name,item,signature) in enumerate(plan):
        if index:
            print('Waiting sixty-second request interval; no audio activity',flush=True)
            time.sleep(args.interval)
        result=lookup(signature)
        record={'case':name,'clip_s':args.seconds,'offset_s':args.offset,
                'at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'expected_title':item['expected_title'],'expected_artist':item['expected_artist'],
                'expected_album':item['expected_album'],'fingerprint_cpu_ms':signature['cpu_ms'],**result}
        records.append(record);output.write_text(json.dumps(records,ensure_ascii=False,indent=2))
        print(json.dumps(record,ensure_ascii=False),flush=True)
        if result['http_status']!=200 or result.get('retry_after'):
            print('Service cooldown/error: no more requests; stop run',flush=True)
            break
    print('Evidence:',output,flush=True)


if __name__=='__main__':
    main()
