import importlib.util
import io
import hashlib
import json
import tempfile
import sys
from pathlib import Path
import unittest
from unittest.mock import patch
import urllib.error
spec=importlib.util.spec_from_file_location('catalogue_check',Path(__file__).with_name('catalogue_check.py'))
check=importlib.util.module_from_spec(spec);spec.loader.exec_module(check)
class Response(io.BytesIO):
    status=200
    headers={}
class CatalogueTests(unittest.TestCase):
    def test_extracts_song_without_audio_download(self):
        response=Response(b'{"matches":[{}],"track":{"title":"Take Five","subtitle":"Dave Brubeck","sections":[{"metadata":[{"title":"Album","text":"Time Out"}]}]}}')
        with patch.object(check.urllib.request,'build_opener') as factory:
            factory.return_value.open.return_value=response
            result=check.lookup({'uri':'data:audio/vnd.shazam.sig;base64,abc','samplems':4000})
            request=factory.return_value.open.call_args.args[0]
            self.assertTrue(request.full_url.startswith('https://amp.shazam.com/'))
            self.assertEqual(factory.return_value.open.call_args.kwargs['timeout'],20)
        self.assertEqual(result['title'],'Take Five');self.assertEqual(result['album'],'Time Out')
        self.assertEqual(result['matches'],1)
    def test_rate_limit_and_retry_header_are_preserved(self):
        error=urllib.error.HTTPError('https://amp.shazam.com/',429,'Too Many Requests',{'Retry-After':'120'},None)
        with patch.object(check.urllib.request,'build_opener') as factory:
            factory.return_value.open.side_effect=error
            result=check.lookup({'uri':'data:audio/vnd.shazam.sig;base64,abc','samplems':4000})
        self.assertEqual(result['http_status'],429);self.assertEqual(result['retry_after'],'120')
        self.assertIn('stop',result['error'])
    def test_oversized_body_and_invalid_json_are_failures(self):
        for payload in (b'x'*(check.LIMIT+1),b'not json'):
            with patch.object(check.urllib.request,'build_opener') as factory:
                factory.return_value.open.return_value=Response(payload)
                result=check.lookup({'uri':'data:audio/vnd.shazam.sig;base64,abc','samplems':4000})
            self.assertEqual(result['http_status'],0);self.assertIn('error',result)
    def test_main_stops_after_rate_limit_without_second_request(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'results/signatures').mkdir(parents=True)
            wav=root/'fixture.wav';wav.write_bytes(b'unit fixture')
            cases=[]
            for name in ('jazz','classical'):
                cases.append({'id':name,'wav':str(wav),'wav_sha256':hashlib.sha256(wav.read_bytes()).hexdigest(),
                              'expected_title':name,'expected_artist':'fixture','expected_album':'fixture'})
                (root/'results/signatures'/(name+'-0-4.json')).write_text(json.dumps({'accepted':True,'uri':'data:audio/vnd.shazam.sig;base64,abc','samplems':4000,'cpu_ms':1}))
            (root/'manifest.json').write_text(json.dumps(cases))
            with patch.object(sys,'argv',
                              ['catalogue_check','--benchmark-dir',str(root),'--seconds','4','--cases','jazz','classical']), \
                 patch.object(check,'lookup',return_value={'http_status':429,'lookup_ms':1,'retry_after':'120'}) as lookup, \
                 patch.object(check.time,'sleep') as sleep,patch('sys.stdout',new_callable=io.StringIO):
                check.main()
            self.assertEqual(lookup.call_count,1);sleep.assert_not_called()
    def test_redirect_is_rejected(self):
        self.assertIsNone(check.NoRedirect().redirect_request(None,None,302,'Found',{},'https://elsewhere.test/'))
if __name__=='__main__':unittest.main()
