import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Execute the real runner with fake HTTP responses; no API keys or network calls.
function runReview(scenario) {
  const temp = mkdtempSync(join(tmpdir(), 'waveform-review-'));
  try {
    const event = join(temp, 'event.json');
    writeFileSync(event, JSON.stringify({pull_request: {
      number: 1, title: 'A change', body: '', head: {sha: 'abc', ref: scenario === 'dependabot' ? 'dependabot/test' : 'feature/test'},
      user: {login: scenario === 'dependabot' ? 'dependabot[bot]' : 'developer'},
    }}));
    const loader = join(temp, 'fake.mjs');
    writeFileSync(loader, `
      const scenario = ${JSON.stringify(scenario)};
      globalThis.fetch = async (url, options = {}) => {
        if (url.includes('api.openai.com')) {
          console.log('MODEL_REQUEST=' + JSON.stringify(JSON.parse(options.body)));
          return Response.json({status: scenario === 'incomplete' ? 'incomplete' : 'completed',
            output_text: scenario === 'invalid' ? 'unparseable' : JSON.stringify({
              summary: 'Reviewed', findings: [], testsVerification: ['Not run'],
              risksFollowups: ['Hardware validation remains']})});
        }
        if (options.method === 'POST') {
          console.log('POSTED=' + options.body);
          return Response.json({});
        }
        if (url.includes('/reviews?')) return Response.json([]);
        if (options.headers?.Accept === 'application/vnd.github.v3.diff') return new Response('diff');
        return Response.json({head: {sha: scenario === 'stale' ? 'def' : 'abc'}});
      };
    `);
    return spawnSync(process.execPath, ['--import', loader,
      fileURLToPath(new URL('./codex-pr-review.mjs', import.meta.url))], {
      cwd: fileURLToPath(new URL('../../', import.meta.url)),
      env: {...process.env, GITHUB_EVENT_PATH: event, GITHUB_REPOSITORY: 'test/test',
        GITHUB_TOKEN: 'fake', OPENAI_API_KEY: scenario === 'missing-key' ? '' : 'fake',
        CODEX_REVIEW_MODEL: 'gpt-5.5', CODEX_REVIEW_REASONING_EFFORT: 'low'},
      encoding: 'utf8',
    });
  } finally {
    rmSync(temp, {recursive: true, force: true});
  }
}

test('posts a review using Voxa workflow model and reasoning settings', () => {
  const result = runReview('valid');
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /POSTED=.*codex-review:abc/);
  const request = JSON.parse(result.stdout.split('\n').find(line => line.startsWith('MODEL_REQUEST=')).slice('MODEL_REQUEST='.length));
  assert.equal(request.model, 'gpt-5.5');
  assert.equal(request.reasoning.effort, 'low');
});

test('skips Dependabot PRs as Voxa does', () => {
  const result = runReview('dependabot');
  assert.equal(result.status, 0, result.stderr);
  assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
});

test('runner fails when its required API key is absent', () => {
  const result = runReview('missing-key');
  assert.notEqual(result.status, 0);
  assert.doesNotMatch(result.stdout, /POSTED=/);
});

test('posts Voxa fallback when the response does not meet the output contract', () => {
  const result = runReview('invalid');
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /POSTED=/);
  assert.match(result.stderr, /posting fallback review body/);
});
