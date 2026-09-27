import unittest
from policy import mode, timer, automatic_install

class PolicyTests(unittest.TestCase):
    def test_production_checks_monthly_without_automatic_installation(self):
        self.assertEqual(mode({}), 'production')
        self.assertIn('OnCalendar=monthly', timer('production'))
        self.assertNotIn('OnBootSec',timer('production'))
        self.assertFalse(automatic_install({}, 'check'))
        with self.assertRaises(ValueError):automatic_install({}, 'sync-main')

    def test_development_automatically_follows_validated_releases(self):
        config={'deployment_mode':'development'}
        self.assertTrue(automatic_install(config,'sync-main'))
        self.assertFalse(automatic_install(config,'check'))
        self.assertIn('OnUnitActiveSec=1min',timer('development'))
        self.assertIn('Unit=waveform-update-sync-main.service',timer('development'))

    def test_unknown_mode_cannot_enable_automatic_updates(self):
        with self.assertRaises(ValueError):mode({'deployment_mode':'automatic'})
        with self.assertRaises(ValueError):timer('automatic')

class AgentPolicyTests(unittest.TestCase):
    def run_agent(self, selected, failed=False):
        import json,tempfile
        from pathlib import Path
        from unittest.mock import patch
        import agent
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'updater.json').write_text(json.dumps({'deployment_mode':selected,'serial':'fake'}))
            (root/'update-public.pem').write_text('test fixture')
            if failed:(root/'failed-automatic.json').write_text('{"sequence":2}')
            with patch.object(agent,'ROOT',root),patch.object(agent,'CONFIG',root), \
                 patch.object(agent.platform,'machine',return_value='aarch64'), \
                 patch.object(agent.sys,'version_info',(3,13)), \
                 patch.object(agent.sys,'argv',['agent','sync-main']), \
                 patch.object(agent,'recover'),patch.object(agent,'prune'),patch.object(agent,'status'), \
                 patch.object(agent,'candidate',return_value={'sequence':2,'version':'v1.2.3'}) as candidate, \
                 patch.object(agent,'install') as install:
                if selected=='production':
                    with self.assertRaises(ValueError):agent.main()
                    candidate.assert_not_called();install.assert_not_called()
                else:
                    agent.main()
                    self.assertEqual(install.call_count,0 if failed else 1)
    def test_agent_installs_on_development_device_without_phone_request(self):
        self.run_agent('development')
    def test_agent_never_autoinstalls_on_customer_device(self):
        self.run_agent('production')
    def test_agent_does_not_retry_failed_version_automatically(self):
        self.run_agent('development',failed=True)
