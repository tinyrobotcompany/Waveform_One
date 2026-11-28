import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {join} from 'node:path';
import {repository} from './release-fixture.mjs';

test('release and changeset callers use Node 22 without the Python planner', () => {
  const release = readFileSync(join(repository, '.github/workflows/release.yml'), 'utf8');
  const checks = readFileSync(join(repository, '.github/workflows/pr-checks.yml'), 'utf8');
  assert.equal(release.match(/node scripts\/release\.mjs plan --output/g)?.length, 2);
  assert.match(checks, /node scripts\/release\.mjs check "\$BASE_SHA"/);
  for (const workflow of [release, checks]) {
    assert.doesNotMatch(workflow, /python3 scripts\/release\.py/);
    assert.match(workflow, /actions\/setup-node@[a-f0-9]{40}/);
    assert.match(workflow, /node-version: '22'/);
  }
});

test('draft release refreshes evidence only after downloaded asset verification', () => {
  const workflow = readFileSync(join(repository, '.github/workflows/release.yml'), 'utf8');
  const publish = 'gh release edit "$tag" --notes-file /tmp/release-notes.md --draft=false --latest';
  const verify = 'scripts/verify-release-assets.py --dist "$RELEASE_DIST" --downloaded';
  assert.ok(workflow.includes(publish));
  assert.ok(workflow.includes(verify));
  assert.ok(workflow.indexOf(verify) < workflow.indexOf(publish));
  assert.match(workflow, /targetCommitish --jq \.targetCommitish\)" = "\$GITHUB_SHA"/);
  assert.match(workflow, /commits\/\$tag" --jq \.sha\)" = "\$GITHUB_SHA"/);
});
