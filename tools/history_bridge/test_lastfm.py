import hashlib
import unittest
from unittest.mock import patch, MagicMock
from urllib.parse import urlsplit, parse_qs
import os
import tempfile
from pathlib import Path
from lastfm import Queue, eligible, signed, read_config, request

class LastFmTests(unittest.TestCase):
    def setUp(self):
        self.config = dict(api_key='api',secret='secret',session='session')
        self.account = hashlib.sha256(b'session').hexdigest()
        self.document = dict(schema=1,session='boot',entries=[dict(id=1,title='Song',artist='Artist',album='Album',duration_seconds=120,observed_seconds=60,started=1800000000)])
    def test_private_files(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'account.json';path.write_text('{}');path.chmod(0o644)
            with self.assertRaises(ValueError):read_config(path)
            with self.assertRaises(ValueError):Queue(path)
            path.chmod(0o600);self.assertEqual(read_config(path),{})
            link=Path(directory)/'link';link.symlink_to(path)
            with self.assertRaises(OSError):read_config(link)

    def test_http_method_and_signed_fields(self):
        with patch('lastfm.urllib.request.build_opener') as factory:
            response=MagicMock()
            response.__enter__.return_value.read.return_value=b'{}'
            factory.return_value.open.return_value=response
            request(dict(method='auth.getToken'),self.config,post=False)
            req=factory.return_value.open.call_args.args[0]
            self.assertEqual(req.get_method(),'GET')
            self.assertIsNone(req.data)
            fields=parse_qs(urlsplit(req.full_url).query)
            self.assertEqual(fields['method'],['auth.getToken'])
            self.assertIn('api_sig',fields)
            self.assertNotIn('secret',fields)
            request(dict(method='track.scrobble',track='A & B'),self.config)
            req=factory.return_value.open.call_args.args[0]
            self.assertEqual(req.get_method(),'POST')
            self.assertEqual(urlsplit(req.full_url).query,'')
            self.assertEqual(parse_qs(req.data.decode())['track'],['A & B'])

    def test_signature(self):
        expected=hashlib.md5('api_keyapimethodauth.getSessiontokenxsecret'.encode()).hexdigest()
        self.assertEqual(signed(dict(token='x',method='auth.getSession',api_key='api'),'secret')['api_sig'],expected)
    def test_eligibility(self):
        self.assertEqual(len(eligible(self.document,1800001000)),1)
        for field,value in [('duration_seconds',30),('observed_seconds',59),('started',0),('started',1800002000)]:
            changed=dict(self.document,entries=[dict(self.document['entries'][0],**{field:value})])
            self.assertEqual(eligible(changed,1800001000),[])
        self.document['entries'][0]['duration_seconds']=float('nan')
        with self.assertRaises(ValueError):eligible(self.document,1800001000)
    def test_dedup_and_accepted(self):
        queue=Queue(':memory:');entries=eligible(self.document,1800001000)
        queue.add(self.account,entries);queue.add(self.account,entries)
        calls=[]
        def transport(params,config):
            calls.append(params);return {'scrobbles':{'@attr':{'accepted':'1','ignored':'0'}}}
        self.assertEqual(queue.send(self.config,transport,1800001000),{'accepted':1})
        queue.send(self.config,transport,1800001001);self.assertEqual(len(calls),1)
    def test_timeout_not_retried(self):
        queue=Queue(':memory:');queue.add(self.account,eligible(self.document,1800001000))
        calls=[]
        def transport(*args):calls.append(1);raise OSError()
        self.assertEqual(queue.send(self.config,transport,1800001000),{'uncertain':1})
        queue.send(self.config,transport,1800002000);self.assertEqual(len(calls),1)
    def test_retry_and_rejection(self):
        queue=Queue(':memory:');queue.add(self.account,eligible(self.document,1800001000))
        self.assertEqual(queue.send(self.config,lambda *a:dict(error=11),1800001000),{'pending':1})
        self.assertEqual(queue.send(self.config,lambda *a:dict(error=6),1800001001),{'pending':1})
        self.assertEqual(queue.send(self.config,lambda *a:{'scrobbles':{'@attr':{'accepted':'0','ignored':'1'}}},1800001061),{'rejected':1})
    def test_account_isolation(self):
        queue=Queue(':memory:');queue.add('other-account',eligible(self.document,1800001000))
        self.assertEqual(queue.send(self.config,lambda *a:self.fail('must not send'),1800001000),{})
if __name__=='__main__':unittest.main()
