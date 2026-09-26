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
      number: 1, title: 'A change', body: '', head: {sha: 'abc', ref: 'dependabot/test'},
      user: {login: 'dependabot[bot]'},
    }}));
    const loader = join(temp, 'fake.mjs');
    writeFileSync(loader, `
      const scenario = ${JSON.stringify(scenario)};
      globalThis.fetch = async (url, options = {}) => {
        if (url.includes('api.openai.com')) {
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
        GITHUB_TOKEN: 'fake', OPENAI_API_KEY: scenario === 'missing-key' ? '' : 'fake'},
      encoding: 'utf8',
    });
  } finally {
    rmSync(temp, {recursive: true, force: true});
  }
}

test('reviews dependency PRs and attaches comments to the reviewed commit', () => {
  const result = runReview('valid');
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /POSTED=.*"commit_id":"abc"/);
});

for (const scenario of ['missing-key', 'incomplete', 'invalid', 'stale']) {
  test(`fails without posting a misleading review: ${scenario}`, () => {
    const result = runReview(scenario);
    assert.notEqual(result.status, 0);
    assert.doesNotMatch(result.stdout, /POSTED=/);
  });
}
