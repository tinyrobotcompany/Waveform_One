import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('release', Path(__file__).parents[1] / 'release.py')
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_changeset_requires_package_bump_and_notes(self):
        self.assertEqual(release.parse_changeset('---\n"waveform-one": minor\n---\nQuieter music works.\n'),
                         ('minor', 'Quieter music works.'))
        for invalid in ('hello', '---\nother: patch\n---\nNotes',
                        '---\n"waveform-one": patch\n---\n',
                        '---\n"waveform-one": banana\n---\nNotes'):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                release.parse_changeset(invalid)

    def test_highest_bump_wins(self):
        self.assertEqual(release.bump('1.2.3', ['patch', 'minor']), '1.3.0')
        self.assertEqual(release.bump('1.2.3', ['major', 'minor']), '2.0.0')
        self.assertEqual(release.bump('0.1.0', ['patch']), '0.1.1')


class GitReleaseTests(unittest.TestCase):
    def setUp(self):
        # Also safe when this suite is invoked directly from a Git hook.
        clean_env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
        environment = patch.dict(os.environ, clean_env, clear=True)
        environment.start()
        self.addCleanup(environment.stop)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.email', 'test@example.com')
        self.git('config', 'user.name', 'Test')
        readme = (Path(__file__).parents[2] / 'README.md').read_text()
        self.notice_readme = readme[readme.index('<!-- privacy-notice:start -->'):]
        self.commit('README.md', self.notice_readme)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], text=True).strip()

    def commit(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        self.git('add', '.')
        self.git('commit', '-qm', 'test')

    def test_initial_release_rerun_and_next_release(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": minor\n---\nInitial firmware.\n')
        plan = release.plan(self.root)
        self.assertEqual(plan['tag'], 'v0.1.0')
        self.assertIn('Initial firmware.', plan['notes'])
        self.assertIn('Waveform One does not save audio recordings on the device.', plan['notes'])
        self.assertIn('Clips may include nearby voices', plan['notes'])
        notice = release.privacy_notice(self.root, plan['commit'])
        self.assertTrue(plan['notes'].endswith(notice + '\n'))
        self.assertEqual(plan['notes'].count(notice), 1)
        self.git('tag', plan['tag'])
        self.assertEqual(release.plan(self.root), plan)
        self.commit('README.md', 'Docs only\n' + self.notice_readme)
        self.assertIsNone(release.plan(self.root))
        self.commit('.changeset/fix.md', '---\n"waveform-one": patch\n---\nQuiet audio fix.\n')
        plan = release.plan(self.root)
        self.assertEqual(plan['tag'], 'v0.1.1')
        self.assertNotIn('Initial firmware.', plan['notes'])
        self.assertEqual(plan['notes'].count('## Microphone and privacy'), 1)
        self.assertTrue(plan['notes'].endswith(notice + '\n'))

    def test_release_rejects_missing_notice_and_uses_committed_notice(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": patch\n---\nFix.\n')
        expected = release.plan(self.root)
        (self.root / 'README.md').write_text('Uncommitted edit without notice')
        self.assertEqual(release.plan(self.root), expected)
        self.commit('README.md', 'Committed removal of notice')
        with self.assertRaisesRegex(ValueError, 'privacy notice'):
            release.plan(self.root)

    def test_release_rejects_empty_duplicate_or_unclosed_notice(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": patch\n---\nFix.\n')
        for notice in ['<!-- privacy-notice:start -->\n<!-- privacy-notice:end -->',
                       self.notice_readme + self.notice_readme,
                       self.notice_readme.replace('<!-- privacy-notice:end -->', '')]:
            with self.subTest(notice=notice[:60]):
                self.commit('README.md', notice)
                with self.assertRaisesRegex(ValueError, 'privacy notice'):
                    release.plan(self.root)

    def test_firmware_change_needs_new_changeset(self):
        self.commit('README.md', 'Initial')
        base = self.git('rev-parse', 'HEAD')
        self.commit('firmware/main.cpp', 'new code')
        with self.assertRaises(ValueError):
            release.check(self.root, base)
        self.commit('.changeset/fix.md', '---\n"waveform-one": patch\n---\nFix.\n')
        release.check(self.root, base)

    def test_consumed_changesets_cannot_be_rewritten(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": minor\n---\nInitial.\n')
        base = self.git('rev-parse', 'HEAD')
        self.commit('.changeset/initial.md', '---\n"waveform-one": major\n---\nChanged.\n')
        with self.assertRaises(ValueError):
            release.check(self.root, base)


if __name__ == '__main__':
    unittest.main()
