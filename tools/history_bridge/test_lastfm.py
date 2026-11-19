import hashlib
import unittest
from lastfm import Queue, eligible, signed

class LastFmTests(unittest.TestCase):
    def setUp(self):
        self.config = dict(api_key='api',secret='secret',session='session')
        self.account = hashlib.sha256(b'session').hexdigest()
        self.document = dict(schema=1,session='boot',entries=[dict(id=1,title='Song',artist='Artist',album='Album',duration_seconds=120,observed_seconds=60,started=1800000000)])
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
