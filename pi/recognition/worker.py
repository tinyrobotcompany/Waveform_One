#!/usr/bin/env python3
"""Local ShazamIO adapter. Audio stays in memory; only fingerprints go to Shazam."""
import asyncio
import io
import json
import os
from pathlib import Path
import time
import urllib.request
from urllib.parse import urlparse
import wave


def metadata(result):
    track = result.get('track')
    if not isinstance(track, dict) or not isinstance(track.get('title'), str) or not track['title'].strip():
        return None
    album = ''
    for section in track.get('sections', []):
        for item in section.get('metadata', []):
            if item.get('title', '').lower() == 'album':
                album = item.get('text', '')
    art = track.get('images', {}).get('coverart')
    url = urlparse(art) if isinstance(art, str) else None
    if not url or url.scheme != 'https' or not url.netloc or url.username or url.password or len(art) > 2048:
        art = None
    def text(value):
        return value[:512] if isinstance(value, str) else ''
    return dict(title=text(track['title']), artist=text(track.get('subtitle')), album=text(album), artwork_url=art)


def wav_clip(pcm):
    if len(pcm) != 256000:
        raise ValueError('Incomplete audio clip')
    # Apply a single gain to the whole clip: improve quiet mic recordings without
    # changing their dynamics. The LED's audio pipeline is not affected.
    import array
    import sys
    samples = array.array('h', pcm)
    if sys.byteorder != 'little':
        samples.byteswap()
    peak = max(abs(s) for s in samples)
    gain = min(32.0, 26000 / peak) if peak else 1.0
    if gain > 1:
        samples = array.array('h', (int(s * gain) for s in samples))
    if sys.byteorder != 'little':
        samples.byteswap()
    output = io.BytesIO()
    with wave.open(output, 'wb') as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(16000)
        audio.writeframes(samples.tobytes())
    return output.getvalue()


class RecognitionState:
    def __init__(self):
        self.session = None
        self.track = None
        self.matched_at = 0
        self.status = 'waiting'
        self.next_attempt = 0
        self.failures = 0

    def clear(self):
        self.session = None
        self.track = None
        self.status = 'waiting'
        # A capture failure can disconnect the ESP. Keep the retry deadline
        # across that transition instead of retrying on every reconnect.
        if not self.failures:
            self.next_attempt = 0

    def begin(self, session, now):
        if self.session != session:
            self.track = None
        self.session = session
        self.status = 'listening'
        self.next_attempt = now + 2

    def finish(self, track, session, now):
        if self.session != session:
            return
        self.track = track
        self.matched_at = now
        self.status = 'matched' if track else 'no_match'
        self.failures = 0
        self.next_attempt = now + 2

    def fail(self, now):
        self.status = 'unavailable'
        self.failures += 1
        self.next_attempt = now + min(300, 30 * 2 ** min(self.failures, 4))

    def view(self, session, now):
        valid = self.session == session and now - self.matched_at < 90
        return dict(session=session, updated_at=int(now), status=self.status,
                    track=self.track if valid else None)


async def run():
    from shazamio import Shazam
    directory = Path(os.environ.get('WAVEFORM_CONFIG_DIR', Path.home() / '.config/waveform-one'))
    token = (directory / 'remote-token').read_text().strip()
    state = RecognitionState()
    shazam = Shazam()
    def request(path, post=False):
        req = urllib.request.Request('http://127.0.0.1:8080' + path,
                                     data=b'' if post else None,
                                     headers={'Authorization': 'Bearer ' + token})
        with urllib.request.urlopen(req, timeout=25 if post else 5) as response:
            return response.read(256001 if post else 32768)
    async def device():
        return json.loads(await asyncio.to_thread(request, '/api/state'))['device']
    def publish(session):
        temporary = directory / 'recognition.tmp'
        temporary.write_text(json.dumps(state.view(session, time.time())))
        temporary.replace(directory / 'recognition.json')
    while True:
        try:
            current = await device()
            session = current['session']
            now = time.time()
            if current['phase'] != 'playing':
                state.clear()
                publish(session)
            elif now >= state.next_attempt:
                state.begin(session, now)
                publish(session)
                try:
                    pcm = await asyncio.to_thread(request, '/api/capture', True)
                    current = await device()
                    if current['phase'] != 'playing' or current['session'] != session:
                        state.clear()
                        publish(current['session'])
                        continue
                    state.status = 'recognizing'
                    publish(session)
                    result = await asyncio.wait_for(shazam.recognize(wav_clip(pcm)), timeout=25)
                    current = await device()
                    if current['phase'] == 'playing' and current['session'] == session:
                        state.finish(metadata(result), session, time.time())
                        print('Recognition: ' + ('matched' if state.track else 'no match'), flush=True)
                    else:
                        state.clear()
                except Exception as error:
                    # No credentials, audio or untrusted provider response in logs.
                    state.fail(time.time())
                    print('Recognition retry: ' + type(error).__name__, flush=True)
                finally:
                    pcm = None  # release the audio even after a failed request
                publish(session)
            else:
                # Heartbeat lets the display distinguish a stopped worker.
                publish(session)
        except Exception as error:
            print('Waiting for display: ' + type(error).__name__, flush=True)
        await asyncio.sleep(1)


if __name__ == '__main__':
    asyncio.run(run())
