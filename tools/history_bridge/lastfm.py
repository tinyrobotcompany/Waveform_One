"""Opt-in desktop bridge for exported Waveform history. No device/account discovery."""
import argparse
import getpass
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import sqlite3
import stat
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://ws.audioscrobbler.com/2.0/'


def signed(params, secret):
    payload = {k: str(v) for k, v in params.items()}
    text = ''.join(k + payload[k] for k in sorted(payload) if k not in ('format', 'api_sig'))
    payload['api_sig'] = hashlib.md5((text + secret).encode('utf-8')).hexdigest()
    payload['format'] = 'json'
    return payload


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def request(params, config, post=True):
    fields = signed(dict(params, api_key=config['api_key']), config['secret'])
    encoded = urllib.parse.urlencode(fields).encode()
    url = API if post else API + '?' + encoded.decode()
    req = urllib.request.Request(url, data=encoded if post else None,
                                 headers={'User-Agent': 'WaveformOneHistory/1',
                                          'Content-Type': 'application/x-www-form-urlencoded'})
    # Never follow redirects carrying account credentials.
    with urllib.request.build_opener(NoRedirect()).open(req, timeout=20) as response:
        body = response.read(262145)
        if len(body) > 262144:
            raise ValueError('Oversized Last.fm response')
        return json.loads(body)


def eligible(document, now):
    if document.get('schema') != 1 or not isinstance(document.get('session'), str) or not document['session']:
        raise ValueError('Unsupported history export')
    entries = document.get('entries')
    if not isinstance(entries, list) or len(entries) > 50:
        raise ValueError('Expected at most 50 history entries')
    output = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError('Invalid entry')
        for field in ('title', 'artist', 'album'):
            if not isinstance(entry.get(field), str) or len(entry[field]) > 513:
                raise ValueError('Invalid metadata')
        duration, observed, started = (entry.get(k) for k in ('duration_seconds', 'observed_seconds', 'started'))
        if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v)
               for v in (duration, observed, started)):
            raise ValueError('Invalid timing')
        if not isinstance(entry.get('id'), int) or isinstance(entry['id'], bool) or entry['id'] < 1:
            raise ValueError('Invalid entry ID')
        if not entry['title'] or not entry['artist'] or not 30 < duration <= 86400:
            continue
        if not min(duration / 2, 240) <= observed <= 86400 or not 1700000000 <= started <= now:
            continue
        item = dict(entry)
        item['event'] = hashlib.sha256((document['session'] + ':' + str(entry['id'])).encode()).hexdigest()
        output.append(item)
    return sorted(output, key=lambda item: item['started'])


def private_stat(info):
    if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise ValueError('Account and queue files must be owned by you with mode 0600')


def read_config(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd) as source:
        private_stat(os.fstat(source.fileno()))
        return json.load(source)


class Queue:
    def __init__(self, path):
        self.lock_fd = None
        if str(path) != ':memory:':
            self.lock_fd = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
            try:
                private_stat(os.fstat(self.lock_fd))
                fcntl.flock(self.lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except Exception:
                os.close(self.lock_fd)
                raise
        self.db = sqlite3.connect(path)
        if str(path) != ':memory:':
            os.chmod(path, 0o600)
        self.db.execute('CREATE TABLE IF NOT EXISTS events (account TEXT, event TEXT, payload TEXT, state TEXT, next_attempt REAL DEFAULT 0, attempts INTEGER DEFAULT 0, PRIMARY KEY(account,event))')
        # A crash after a send has an ambiguous result, never blindly resubmit it.
        self.db.execute("UPDATE events SET state='uncertain' WHERE state='sending'")
        self.db.commit()

    def add(self, account, entries):
        for entry in entries:
            if self.db.execute('SELECT 1 FROM events WHERE account=? AND event=?', (account, entry['event'])).fetchone():
                continue
            if self.db.execute('SELECT count(*) FROM events').fetchone()[0] >= 1000:
                raise ValueError('Queue/receipt limit reached (1000); export or delete old records explicitly')
            self.db.execute("INSERT INTO events(account,event,payload,state) VALUES(?,?,?,'pending')", (account, entry['event'], json.dumps(entry)))
        self.db.commit()

    def send(self, config, transport=request, now=None):
        now = time.time() if now is None else now
        account = hashlib.sha256(config['session'].encode()).hexdigest()
        rows = self.db.execute("SELECT event,payload,attempts FROM events WHERE account=? AND state='pending' AND next_attempt<=? ORDER BY json_extract(payload,'$.started') LIMIT 50", (account, now)).fetchall()
        for event, payload, attempts in rows:
            entry = json.loads(payload)
            self.db.execute("UPDATE events SET state='sending' WHERE account=? AND event=?", (account, event)); self.db.commit()
            params = dict(method='track.scrobble', sk=config['session'], artist=entry['artist'], track=entry['title'], album=entry['album'], timestamp=int(entry['started']), duration=int(entry['duration_seconds']))
            state, delay = 'uncertain', 0
            try:
                response = transport(params, config)
                if 'error' in response:
                    code = int(response['error'])
                    if code in (11, 16, 29):
                        state, delay = 'pending', min(86400, 60 * 2 ** min(attempts, 10))
                    elif code == 9:
                        state = 'blocked'
                    else:
                        state = 'rejected'
                else:
                    stats = response.get('scrobbles', {}).get('@attr', {})
                    if int(stats.get('accepted', -1)) == 1 and int(stats.get('ignored', -1)) == 0:
                        state = 'accepted'
                    elif int(stats.get('ignored', -1)) == 1:
                        state = 'rejected'
            except (OSError, ValueError, TypeError, KeyError):
                # The server may have accepted a request before a timeout/disconnect.
                state = 'uncertain'
            self.db.execute('UPDATE events SET state=?,next_attempt=?,attempts=attempts+1 WHERE account=? AND event=?', (state, now + delay, account, event)); self.db.commit()
            if state in ('pending', 'blocked', 'uncertain'):
                break
        return dict(self.db.execute('SELECT state,count(*) FROM events WHERE account=? GROUP BY state', (account,)))


def private_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as handle:
        json.dump(data, handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=Path.home() / '.config/waveform-history/lastfm.json')
    parser.add_argument('--queue', type=Path, default=Path.home() / '.config/waveform-history/queue.sqlite')
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('login')
    preview = sub.add_parser('preview'); preview.add_argument('export', type=Path)
    enqueue = sub.add_parser('enqueue'); enqueue.add_argument('export', type=Path); enqueue.add_argument('--confirm-listened', action='store_true', required=True)
    sub.add_parser('submit')
    sub.add_parser('status')
    resolve = sub.add_parser('resolve'); resolve.add_argument('event'); resolve.add_argument('--outcome', choices=['accepted', 'not-delivered', 'discard'], required=True)
    sub.add_parser('delete-local-data')
    args = parser.parse_args()
    if args.command == 'login':
        if args.config.exists():
            raise ValueError('Config already exists; remove it explicitly before changing accounts')
        config = dict(api_key=getpass.getpass('Last.fm application API key: '), secret=getpass.getpass('Last.fm application shared secret: '))
        result = request(dict(method='auth.getToken'), config, post=False)
        token = result.get('token')
        if not isinstance(token, str) or len(token) != 32:
            raise ValueError('Last.fm did not issue an authorization token')
        print('Authorize your account at: https://www.last.fm/api/auth/?' + urllib.parse.urlencode(dict(api_key=config['api_key'], token=token)))
        input('After approving on Last.fm, press Enter here: ')
        result = request(dict(method='auth.getSession', token=token), config, post=False)
        session = result.get('session', {}).get('key')
        if not isinstance(session, str) or not session:
            raise ValueError('Last.fm authorization failed; no credentials saved')
        config['session'] = session
        private_write(args.config, config)
        print('Authorized. No history has been sent.')
        return
    if args.command == 'preview':
        entries = eligible(json.loads(args.export.read_text()), time.time())
        print(json.dumps(entries, indent=2, ensure_ascii=False)); return
    if args.command == 'delete-local-data':
        queue_lock = Queue(args.queue) if args.queue.exists() else None
        if queue_lock:
            queue_lock.db.close()
        for path in (args.config, args.queue):
            path.unlink(missing_ok=True)
        print('Local credentials/queue removed. Export files and Last.fm records are separate; revoke access at Last.fm.'); return
    config = read_config(args.config)
    account = hashlib.sha256(config['session'].encode()).hexdigest()
    args.queue.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    queue = Queue(args.queue)
    if args.command == 'enqueue':
        queue.add(account, eligible(json.loads(args.export.read_text()), time.time()))
        print('Queued locally. Run submit explicitly to send to Last.fm.')
    elif args.command == 'submit':
        print(queue.send(config))
    elif args.command == 'resolve':
        state = {'accepted': 'accepted', 'not-delivered': 'pending', 'discard': 'rejected'}[args.outcome]
        queue.db.execute("UPDATE events SET state=?,next_attempt=0 WHERE account=? AND event=? AND state IN ('uncertain','blocked')", (state, account, args.event)); queue.db.commit()
        print('Resolution saved; requeue only after checking your Last.fm profile.')
    else:
        for event, state in queue.db.execute('SELECT event,state FROM events WHERE account=?', (account,)):
            print(event, state)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        # Never print HTTP request/response objects or credentials in diagnostics.
        raise SystemExit('History operation failed (' + type(error).__name__ + '). Check inputs, authorization and connection.')
