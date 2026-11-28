import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const workflow = readFileSync(new URL('../workflows/codex-pr-review.yaml', import.meta.url), 'utf8');

test('the review runner cannot be skipped when its credential is missing', () => {
  const reviewStep = workflow.split('      - name: Run Codex review\n')[1]?.split('\n      - ')[0];
  assert.ok(reviewStep, 'The single review runner step must exist');
  assert.doesNotMatch(reviewStep, /^\s+if:/m, 'Credential failure belongs in the runner, not a successful workflow skip');
  assert.doesNotMatch(workflow, /detect-openai|skipping Codex review/);
  assert.match(reviewStep, /OPENAI_API_KEY: \$\{\{ secrets\.OPENAI_API_KEY \}\}/);
  assert.match(reviewStep, /run: node \.github\/scripts\/codex-pr-review\.mjs/);
});

test('privileged review executes trusted base code in one review job', () => {
  assert.match(workflow, /pull_request_target:/);
  assert.match(workflow, /ref: \$\{\{ github\.event\.pull_request\.base\.sha \}\}/);
  assert.doesNotMatch(workflow, /ref:.*(?:head\.sha|head\.ref|refs\/pull)/);
  assert.equal((workflow.match(/^    name: Codex Review$/gm) || []).length, 1);
  assert.match(workflow, /persist-credentials: false/);
  assert.doesNotMatch(workflow, /continue-on-error: true/);
});
