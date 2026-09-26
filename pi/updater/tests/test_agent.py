import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import agent
from updates import atomic_json,activate

class FakeEsp:
    calls=[]
    digest='new'
    error=False
    def __init__(self,path):
        if self.error:raise OSError('unplugged')
    def status(self):return dict(digest=self.digest)
    def request(self,command,prefix):self.calls.append(command);return prefix
    def close(self):pass

class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name).resolve()
        self.old=self.root/'old';self.old.mkdir()
        self.new=self.root/'new';self.new.mkdir()
        activate(self.root,self.new)
        atomic_json(self.root/'transaction.json',dict(previous=str(self.old),sequence=2,esp_changed=True,esp_digest='new'))
        FakeEsp.calls=[];FakeEsp.digest='new';FakeEsp.error=False
        self.patches=[patch.object(agent,'ROOT',self.root),patch.object(agent,'Esp',FakeEsp),
                      patch.object(agent,'service'),patch.object(agent,'health'),patch.object(agent,'status')]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.tmp.cleanup()
    def test_interrupted_update_restores_both_components(self):
        agent.recover({'serial':'fake'})
        self.assertEqual((self.root/'current').resolve(),self.old)
        self.assertEqual(FakeEsp.calls,['WFU ROLLBACK'])
        self.assertFalse((self.root/'transaction.json').exists())
    def test_staged_but_not_booted_firmware_is_cancelled(self):
        FakeEsp.digest='old'
        agent.recover({'serial':'fake'})
        self.assertEqual(FakeEsp.calls,['WFU CANCEL'])
    def test_unplugged_esp_restores_pi_and_retains_recovery_journal(self):
        FakeEsp.error=True
        with self.assertRaises(OSError):agent.recover({'serial':'fake'})
        self.assertEqual((self.root/'current').resolve(),self.old)
        self.assertTrue((self.root/'transaction.json').exists())
        agent.service.assert_called_with('start')
    def test_power_loss_after_commit_does_not_undo_successful_update(self):
        atomic_json(self.root/'installed.json',dict(sequence=2,version='v1.0.0'))
        agent.recover({'serial':'fake'})
        self.assertEqual((self.root/'current').resolve(),self.new)
        self.assertEqual(FakeEsp.calls,[])
        agent.service.assert_not_called()
    def test_repeated_recovery_must_not_cancel_a_restored_firmware_twice(self):
        agent.recover({'serial':'fake'});agent.recover({'serial':'fake'})
        self.assertEqual(FakeEsp.calls,['WFU ROLLBACK'])

class InstallTests(unittest.TestCase):
    def test_failed_pi_health_restores_previous_release(self):
        import hashlib,io,tarfile
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();old=root/'old';old.mkdir();activate(root,old)
            archive=io.BytesIO()
            with tarfile.open(fileobj=archive,mode='w:gz') as tar:
                item=tarfile.TarInfo('bin/waveform-display');item.size=3;item.mode=0o755
                tar.addfile(item,io.BytesIO(b'app'))
            data={'pi':archive.getvalue(),'esp':b'esp'}
            m=dict(version='v1.0.0',commit='a'*40,sequence=1,esp_elf_sha256='new',product='waveform-one',assets={
                k:dict(name=agent.NAMES[k],size=len(v),sha256=hashlib.sha256(v).hexdigest()) for k,v in data.items()})
            esp=Mock();esp.status.return_value=dict(digest='old')
            with patch.object(agent,'ROOT',root),patch.object(agent,'status'),patch.object(agent,'service') as services, \
                 patch.object(agent,'health',side_effect=[RuntimeError('unhealthy'),None]), \
                 patch.object(agent,'download',side_effect=list(data.values())), \
                 patch.object(agent.subprocess,'run'),patch.object(agent.os,'sync'), \
                 patch.object(agent.time,'sleep'),patch.object(agent,'Esp',return_value=esp), \
                 patch.object(agent,'connect_healthy',return_value=esp):
                with self.assertRaisesRegex(RuntimeError,'unhealthy'):agent.install(m,{'serial':'fake'})
                self.assertEqual((root/'current').resolve(),old)
                self.assertFalse((root/'installed.json').exists())
                self.assertFalse((root/'transaction.json').exists())
                self.assertNotIn('WFU CONFIRM',[c.args[0] for c in esp.request.call_args_list])
                services.assert_called_with('start')
