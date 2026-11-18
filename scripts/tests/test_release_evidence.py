import subprocess
from pathlib import Path
import unittest
import test_release as fixture

release = fixture.release


class ReleaseEvidenceTests(unittest.TestCase):
    setUp = fixture.GitReleaseTests.setUp
    git = fixture.GitReleaseTests.git
    commit = fixture.GitReleaseTests.commit

    def test_disclosures_survive_first_release_rerun_and_next_release(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": patch\n---\nInitial.\n')
        first = release.plan(self.root)
        self.assertEqual(first['notes'].count(self.evidence.strip()), 1)
        self.git('tag', first['tag'])
        self.assertEqual(release.plan(self.root), first)
        self.commit('.changeset/next.md', '---\n"waveform-one": patch\n---\nNext.\n')
        self.assertEqual(release.plan(self.root)['notes'].count(self.evidence.strip()), 1)

    def test_repository_disclosure_has_both_nonempty_sections(self):
        text = (Path(__file__).parents[2] / 'docs/release-evidence.md').read_text()
        self.assertEqual(release.parse_release_evidence(text), text.strip())


    def test_release_uses_committed_evidence_and_requires_both_nonempty_sections(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": patch\n---\nInitial.\n')
        path = self.root / 'docs/release-evidence.md'
        path.write_text('Uncommitted marketing claim')
        self.assertIn(self.evidence.strip(), release.plan(self.root)['notes'])
        invalid = ['', '## Recognition accuracy\nOnly one section.',
                   self.evidence.replace('Not benchmarked.', ''),
                   self.evidence.replace('Price unverified.', '<!-- TODO -->'),
                   self.evidence + self.evidence,
                   self.evidence.replace('## Recognition service costs', '## Something else')]
        for text in invalid:
            with self.subTest(text=text):
                self.commit('docs/release-evidence.md', text)
                with self.assertRaises(ValueError):
                    release.plan(self.root)
        self.git('rm', 'docs/release-evidence.md')
        self.git('commit', '-qm', 'remove evidence')
        with self.assertRaises(subprocess.CalledProcessError):
            release.plan(self.root)


    def test_updated_committed_disclosure_is_used_on_subsequent_release(self):
        self.commit('.changeset/initial.md', '---\n"waveform-one": patch\n---\nInitial.\n')
        self.git('tag', 'v0.0.1')
        updated = self.evidence.replace('Price unverified.', 'Updated dated commercial evidence.')
        self.commit('docs/release-evidence.md', updated)
        self.commit('.changeset/next.md', '---\n"waveform-one": patch\n---\nNext.\n')
        self.assertIn(updated.strip(), release.plan(self.root)['notes'])


    def test_draft_publication_refreshes_notes_after_verification(self):
        workflow = (Path(__file__).parents[2] / '.github/workflows/release.yml').read_text()
        refresh = 'gh release edit "$tag" --notes-file /tmp/release-notes.md --draft=false --latest'
        self.assertIn(refresh, workflow)
        self.assertLess(workflow.index('python3 scripts/verify-release-assets.py --dist "$RELEASE_DIST" --downloaded'),
                        workflow.index(refresh))

