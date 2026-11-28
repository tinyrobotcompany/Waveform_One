import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Execute the real runner with fake HTTP responses; no API keys or network calls.
function runReview(scenario, configuration = {}) {
  const artifacts = fileURLToPath(new URL('../../.artifacts/', import.meta.url));
  mkdirSync(artifacts, {recursive: true});
  const temp = mkdtempSync(join(artifacts, 'waveform-review-'));
  try {
    const event = join(temp, 'event.json');
    writeFileSync(event, JSON.stringify({pull_request: {
      number: 1, title: 'A change', body: '', head: {sha: 'abc', ref: scenario === 'dependabot' ? 'dependabot/test' : 'feature/test'},
      user: {login: scenario === 'dependabot' ? 'dependabot[bot]' : 'developer'},
    }}));
    const loader = join(temp, 'fake.mjs');
    writeFileSync(loader, `
      import {renderStructuredReviewBody} from ${JSON.stringify(new URL('./codex-pr-review-core.mjs', import.meta.url).href)};
      const scenario = ${JSON.stringify(scenario)};
      const review = {summary: 'Reviewed', findings: [], testsVerification: ['Not run'],
        risksFollowups: ['Hardware validation remains']};
      if (['critical', 'high', 'medium', 'low'].includes(scenario)) review.findings = [{
        severity: scenario[0].toUpperCase() + scenario.slice(1), title: 'Issue',
        evidence: 'file:1', impact: 'Broken behavior', fix: 'Correct it'}];
      if (scenario === 'unknown-severity') review.findings = [{severity: 'Informational',
        title: 'Issue', evidence: 'file:1', impact: 'Broken behavior', fix: 'Correct it'}];
      if (scenario === 'invalid-list') review.testsVerification.push(123);
      if (scenario === 'extra-field') review.unexpected = true;
      const stored = {summary: 'Previous review', findings: [], testsVerification: ['Not run'], risksFollowups: ['Check hardware']};
      if (scenario === 'previous-blocking') stored.findings.push({severity: 'High', title: 'Issue', evidence: 'file:1', impact: 'Broken', fix: 'Fix it'});
      const previous = {user: {login: 'github-actions[bot]', type: 'Bot'}, commit_id: 'abc', state: 'COMMENTED',
        body: '<!-- codex-review:abc -->\\n' + renderStructuredReviewBody(stored) + '\\n<!-- codex-review-result:v1:' + Buffer.from(JSON.stringify(stored)).toString('base64') + ' -->'};
      if (scenario === 'previous-legacy') previous.body = '<!-- codex-review:abc -->\\nNo blocking issues found';
      if (scenario === 'previous-tampered') previous.body = previous.body.replace('Previous review', 'Changed review');
      if (scenario === 'previous-spoof') previous.user = {login: 'developer', type: 'User'};
      if (scenario === 'previous-other-head') previous.commit_id = 'def';
      if (scenario === 'previous-dismissed') previous.state = 'DISMISSED';
      let metadataReads = 0;
      globalThis.fetch = async (url, options = {}) => {
        if (url.includes('api.openai.com')) {
          console.log('MODEL_REQUEST=' + JSON.stringify(JSON.parse(options.body)));
          if (scenario === 'api-error') return new Response('unavailable', {status: 503});
          if (scenario === 'network-error') throw new Error('Network unavailable');
          if (scenario === 'invalid-json') return new Response('{');
          return Response.json({status: scenario === 'incomplete' ? 'incomplete' : 'completed',
            ...(scenario === 'refusal' ? {output: [{type: 'message', role: 'assistant',
              content: [{type: 'refusal', refusal: 'Cannot review'}]}]} : {}),
            output_text: scenario === 'invalid' ? 'unparseable' :
              scenario === 'markdown' ? 'No blocking issues found\\nResidual risks: none' : JSON.stringify(review)});
        }
        if (options.method === 'POST') {
          console.log('POSTED=' + options.body);
          if (scenario === 'posting-error') return new Response('unavailable', {status: 503});
          return Response.json({});
        }
        if (url.includes('/reviews?')) {
          if (scenario === 'previous-second-page' && url.includes('page=2')) return Response.json([previous]);
          if (scenario === 'previous-second-page') return Response.json(Array.from({length: 100}, () => ({body: '', commit_id: 'def'})));
          return Response.json(scenario.startsWith('previous-') ? [previous] : []);
        }
        if (options.headers?.Accept === 'application/vnd.github.v3.diff') return new Response('diff');
        metadataReads++;
        return Response.json({head: {sha: scenario === 'stale-start' || (scenario === 'stale' && metadataReads > 1) ? 'def' : 'abc'}});
      };
    `);
    return spawnSync(process.execPath, ['--import', loader,
      fileURLToPath(new URL('./codex-pr-review.mjs', import.meta.url))], {
      cwd: fileURLToPath(new URL('../../', import.meta.url)),
      env: {...process.env, GITHUB_EVENT_PATH: event, GITHUB_REPOSITORY: 'test/test',
        GITHUB_ACTOR: 'developer', GITHUB_API_URL: 'https://api.github.com',
        GITHUB_TOKEN: scenario === 'missing-github-key' ? '' : 'fake', OPENAI_API_KEY: scenario === 'missing-key' ? '' : 'fake',
        CODEX_REVIEW_MODEL: 'gpt-5.5', CODEX_REVIEW_REASONING_EFFORT: 'low',
        CODEX_REVIEW_DIFF_MAX: '120000', CODEX_REVIEW_MAX_OUTPUT_TOKENS: '6000',
        ...configuration},
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
  assert.match(result.stdout, /POSTED=.*\"commit_id\":\"abc\"/);
  const request = JSON.parse(result.stdout.split('\n').find(line => line.startsWith('MODEL_REQUEST=')).slice('MODEL_REQUEST='.length));
  assert.equal(request.model, 'gpt-5.5');
  assert.equal(request.reasoning.effort, 'low');
});

test('uses the approved default model when no override is supplied', () => {
  const result = runReview('valid', {CODEX_REVIEW_MODEL: ''});
  assert.equal(result.status, 0, result.stderr);
  const request = JSON.parse(result.stdout.split('\n').find(line => line.startsWith('MODEL_REQUEST=')).slice('MODEL_REQUEST='.length));
  assert.equal(request.model, 'gpt-5.5');
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

for (const scenario of ['missing-github-key', 'invalid', 'incomplete', 'invalid-list',
  'unknown-severity', 'extra-field', 'markdown', 'refusal', 'invalid-json', 'api-error', 'network-error', 'stale']) {
  test(`fails closed without publishing an unverified review: ${scenario}`, () => {
    const result = runReview(scenario);
    assert.notEqual(result.status, 0);
    assert.doesNotMatch(result.stdout, /POSTED=/);
  });
}

for (const severity of ['critical', 'high']) {
  test(`${severity} findings are posted against the head and fail the gate`, () => {
    const result = runReview(severity);
    assert.notEqual(result.status, 0);
    assert.match(result.stdout, /POSTED=.*\"commit_id\":\"abc\"/);
    assert.match(result.stderr, /blocking findings/i);
  });
}

for (const severity of ['medium', 'low']) {
  test(`${severity} findings remain visible without blocking the gate`, () => {
    const result = runReview(severity);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /POSTED=/);
  });
}

test('oversized complete diff fails before any paid request or review', () => {
  const result = runReview('valid', {CODEX_REVIEW_DIFF_MAX: '3'});
  assert.notEqual(result.status, 0);
  assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
  assert.match(result.stderr, /split/i);
});

test('a diff exactly at the configured limit is reviewed in full', () => {
  const result = runReview('valid', {CODEX_REVIEW_DIFF_MAX: '4'});
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /MODEL_REQUEST=.*diff/);
});

for (const limit of ['0', '-1', 'invalid', '4tail', '1.5']) {
  test(`invalid diff limit fails before the paid request: ${limit}`, () => {
    const result = runReview('valid', {CODEX_REVIEW_DIFF_MAX: limit});
    assert.notEqual(result.status, 0);
    assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
  });
}

for (const scenario of ['previous-valid', 'previous-second-page']) {
  test(`reuses verified exact-head result without another paid call: ${scenario}`, () => {
    const result = runReview(scenario);
    assert.equal(result.status, 0, result.stderr);
    assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
  });
}
for (const scenario of ['previous-blocking', 'previous-legacy', 'previous-tampered', 'previous-dismissed']) {
  test(`existing review cannot turn an unverified or blocking result green: ${scenario}`, () => {
    const result = runReview(scenario);
    assert.notEqual(result.status, 0);
    assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
  });
}
for (const scenario of ['previous-spoof', 'previous-other-head']) {
  test(`untrusted marker cannot suppress the actual review: ${scenario}`, () => {
    const result = runReview(scenario);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /MODEL_REQUEST=/);
    assert.match(result.stdout, /POSTED=/);
  });
}

test('does not spend on a review whose event head is already stale', () => {
  const result = runReview('stale-start');
  assert.notEqual(result.status, 0);
  assert.doesNotMatch(result.stdout, /MODEL_REQUEST=|POSTED=/);
});

test('failure to publish the review fails the gate', () => {
  const result = runReview('posting-error');
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /GitHub API 503/);
});
