import assert from 'node:assert/strict';
import test from 'node:test';
import * as release from '../release.mjs';
import {fixture, changeset, evidence, noticeReadme} from './release-fixture.mjs';

test('changesets require the package, supported bump and nonempty notes', async () => {
  const {parseChangeset} = await import('../release.mjs');
  assert.deepEqual(parseChangeset('---\n"waveform-one": minor\n---\nQuieter music works.\n'),
    ['minor', 'Quieter music works.']);
  for (const invalid of ['hello', '---\nother: patch\n---\nNotes',
    '---\n"waveform-one": patch\n---\n', '---\n"waveform-one": banana\n---\nNotes']) {
    assert.throws(() => parseChangeset(invalid), /Changeset must contain/);
  }
});

test('initial release, tagged retry and subsequent release preserve exact notes', (t) => {
  const f = fixture(t);
  assert.equal(release.plan(f.root), null);
  f.commit('.changeset/initial.md', changeset('minor', 'Initial firmware.'));
  const first = release.plan(f.root);
  assert.equal(first.tag, 'v0.1.0');
  assert.equal(first.commit, f.git('rev-parse', 'HEAD'));
  assert.match(first.notes, /Initial firmware\./);
  assert.match(first.notes, /Waveform One does not save audio recordings on the device\./);
  assert.match(first.notes, /Clips may include nearby voices/);
  const notice = release.privacyNotice(f.root, first.commit);
  assert.ok(first.notes.endsWith(notice + '\n'));
  assert.equal(first.notes.split(notice).length - 1, 1);
  assert.equal(first.notes.split(evidence.trim()).length - 1, 1);
  f.git('tag', first.tag);
  assert.deepEqual(release.plan(f.root), first);
  f.commit('README.md', 'Docs only\n' + noticeReadme);
  assert.equal(release.plan(f.root), null);
  f.commit('.changeset/fix.md', changeset('patch', 'Quiet audio fix.'));
  const next = release.plan(f.root);
  assert.equal(next.tag, 'v0.1.1');
  assert.doesNotMatch(next.notes, /Initial firmware/);
  assert.equal(next.notes.split('## Microphone and privacy').length - 1, 1);
  assert.equal(next.notes.split(evidence.trim()).length - 1, 1);
  assert.ok(next.notes.endsWith(notice + '\n'));
  f.git('tag', '-a', next.tag, '-m', 'Annotated release');
  assert.deepEqual(release.plan(f.root), next);
});

test('release ignores working-tree claims but rejects committed missing privacy', (t) => {
  const f = fixture(t);
  f.commit('.changeset/initial.md', changeset());
  const expected = release.plan(f.root);
  f.write('README.md', 'Uncommitted claim without notice');
  f.write('docs/release-evidence.md', 'Uncommitted marketing claim');
  assert.deepEqual(release.plan(f.root), expected);
  f.write('docs/release-evidence.md', evidence);
  f.commit('README.md', 'Committed removal of notice');
  assert.throws(() => release.plan(f.root), /privacy notice/);
});

test('empty, duplicate and unclosed privacy notices fail release planning', (t) => {
  const f = fixture(t);
  f.commit('.changeset/initial.md', changeset());
  for (const notice of ['<!-- privacy-notice:start -->\n<!-- privacy-notice:end -->',
    noticeReadme + noticeReadme, noticeReadme.replace('<!-- privacy-notice:end -->', '')]) {
    f.commit('README.md', notice);
    assert.throws(() => release.plan(f.root), /privacy notice/);
  }
});

test('firmware and automation changes require a newly added valid changeset', (t) => {
  const f = fixture(t);
  const base = f.git('rev-parse', 'HEAD');
  f.commit('firmware/main.cpp', 'new code');
  assert.throws(() => release.check(f.root, base), /Add a new/);
  f.commit('.changeset/fix.md', changeset());
  assert.doesNotThrow(() => release.check(f.root, base));
  const nextBase = f.git('rev-parse', 'HEAD');
  f.commit('scripts/tool.mjs', '// new automation');
  assert.throws(() => release.check(f.root, nextBase), /Add a new/);
});

test('backend changes require a new changeset', (t) => {
  const f = fixture(t);
  const base = f.git('rev-parse', 'HEAD');
  f.commit('backend/src/CertificatePolicy.cs', '// new service behavior');
  assert.throws(() => release.check(f.root, base), /Add a new/);
  f.commit('.changeset/backend.md', changeset());
  assert.doesNotThrow(() => release.check(f.root, base));
});

test('docs-only changes need no changeset but still validate disclosure', (t) => {
  const f = fixture(t);
  const base = f.git('rev-parse', 'HEAD');
  f.commit('docs/help.md', 'Help');
  assert.doesNotThrow(() => release.check(f.root, base));
  f.commit('docs/release-evidence.md', 'Invalid evidence');
  assert.throws(() => release.check(f.root, base), /Release evidence/);
});

for (const operation of ['modify', 'delete', 'rename']) {
  test(`existing changesets remain immutable: ${operation}`, (t) => {
    const f = fixture(t);
    f.commit('.changeset/initial.md', changeset('minor', 'Initial.'));
    const base = f.git('rev-parse', 'HEAD');
    if (operation === 'modify') f.commit('.changeset/initial.md', changeset('major', 'Changed.'));
    else {
      if (operation === 'delete') f.git('rm', '.changeset/initial.md');
      else f.git('mv', '.changeset/initial.md', '.changeset/renamed.md');
      f.git('commit', '-qm', 'test: mutate changeset');
    }
    assert.throws(() => release.check(f.root, base), /immutable/);
  });
}

test('version selection is numeric and excludes tags outside head ancestry', (t) => {
  const f = fixture(t);
  f.commit('.changeset/start.md', changeset('minor'));
  f.git('tag', 'v0.9.0');
  f.commit('.changeset/next.md', changeset());
  f.git('tag', 'v0.10.0');
  f.git('tag', 'v99.0.0-beta');
  const branchPoint = f.git('rev-parse', 'HEAD');
  f.git('checkout', '-qb', 'unrelated');
  f.commit('unrelated.txt', 'Separate branch');
  f.git('tag', 'v99.0.0');
  f.git('checkout', '--detach', branchPoint);
  f.commit('.changeset/final.md', changeset());
  assert.equal(release.plan(f.root).tag, 'v0.10.1');
});

test('highest semantic bump wins without losing integer precision', async () => {
  const {bump} = await import('../release.mjs');
  assert.equal(bump('1.2.3', ['patch', 'minor']), '1.3.0');
  assert.equal(bump('1.2.3', ['major', 'minor']), '2.0.0');
  assert.equal(bump('0.1.0', ['patch']), '0.1.1');
  assert.equal(bump('9007199254740993.0.0', ['major']), '9007199254740994.0.0');
});

test('release evidence requires exactly the two nonempty disclosures', async () => {
  const {parseReleaseEvidence} = await import('../release.mjs');
  const evidence = '## Recognition accuracy\n\nNot benchmarked.\n\n## Recognition service costs\n\nPrice unverified.\n';
  assert.equal(parseReleaseEvidence(evidence), evidence.trim());
  for (const invalid of ['', '## Recognition accuracy\nOnly one section.',
    evidence.replace('Not benchmarked.', ''), evidence.replace('Price unverified.', '<!-- TODO -->'),
    evidence + evidence, evidence.replace('## Recognition service costs', '## Something else'),
    'Undocumented preamble\n' + evidence]) {
    assert.throws(() => parseReleaseEvidence(invalid), /Release evidence/);
  }
});
