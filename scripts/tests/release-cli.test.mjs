import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {join} from 'node:path';
import {fixture, changeset, repository, evidence} from './release-fixture.mjs';
import {plan, releaseEvidence} from '../release.mjs';

function cliFixture(t) {
  const f = fixture(t);
  f.write('scripts/release.mjs', readFileSync(join(repository, 'scripts/release.mjs')));
  return f;
}

test('CLI writes null or exact committed release plan and validates changesets', (t) => {
  const f = cliFixture(t);
  const base = f.git('rev-parse', 'HEAD');
  assert.equal(f.cli('plan', '--output', 'plan.json').status, 0);
  assert.equal(readFileSync(join(f.root, 'plan.json'), 'utf8'), 'null\n');
  f.git('add', 'scripts/release.mjs');
  f.git('commit', '-qm', 'test: automation');
  assert.notEqual(f.cli('check', base).status, 0);
  // Keep generated output outside the committed release inputs.
  f.write('.gitignore', 'plan.json\n');
  f.commit('.changeset/release.md', changeset());
  assert.equal(f.cli('check', base).status, 0);
  assert.equal(f.cli('plan', '--output', 'plan.json').status, 0);
  assert.deepEqual(JSON.parse(readFileSync(join(f.root, 'plan.json'))), plan(f.root));
});

test('CLI rejects invalid invocation, invalid git refs and unwritable output', (t) => {
  const f = cliFixture(t);
  for (const args of [[], ['unknown'], ['check'], ['check', 'HEAD', 'extra'],
    ['plan'], ['plan', '--output'], ['plan', '--output', 'x', 'extra'],
    ['check', 'nonexistent-reference'], ['plan', '--output', 'missing/output.json']]) {
    assert.notEqual(f.cli(...args).status, 0, JSON.stringify(args));
  }
});

test('committed invalid or absent evidence blocks plans; updated evidence is included', (t) => {
  const f = fixture(t);
  f.commit('.changeset/release.md', changeset());
  const first = plan(f.root);
  f.git('tag', first.tag);
  f.commit('.changeset/next.md', changeset());
  const updated = evidence.replace('Not benchmarked.', 'Measured with a held-out set.');
  f.commit('docs/release-evidence.md', updated);
  assert.ok(plan(f.root).notes.includes(updated.trim()));
  for (const invalid of ['', evidence + evidence, evidence.replace('Not benchmarked.', '<!-- TODO -->'),
    evidence.replace('## Recognition service costs', '## Wrong section')]) {
    f.commit('docs/release-evidence.md', invalid);
    assert.throws(() => plan(f.root), /Release evidence/);
  }
  f.git('rm', 'docs/release-evidence.md');
  f.git('commit', '-qm', 'test: missing evidence');
  assert.throws(() => plan(f.root));
});

test('repository committed evidence satisfies the release contract', () => {
  assert.ok(releaseEvidence(repository, 'HEAD').includes('## Recognition accuracy'));
});
