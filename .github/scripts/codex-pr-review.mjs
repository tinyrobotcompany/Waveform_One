#!/usr/bin/env node

import fs from 'fs';
import process from 'process';
import {
  buildSystemPrompt,
  buildUserPrompt,
  buildOpenAiErrorDiagnostics,
  buildOpenAiReviewRequestPayload,
  buildOpenAiResponseDiagnostics,
  parseOpenAiReview,
  renderStructuredReviewBody,
  hasBlockingFindings,
  isChangesetReleasePr,
  isDependabotPr,
  loadSkillRubrics,
} from './codex-pr-review-core.mjs';

const eventPath = process.env.GITHUB_EVENT_PATH;
if (!eventPath) {
  console.error('GITHUB_EVENT_PATH is not set.');
  process.exit(1);
}

const event = JSON.parse(fs.readFileSync(eventPath, 'utf8'));
const pr = event.pull_request;
if (!pr) {
  console.error('This workflow must run on a pull_request or pull_request_target event.');
  process.exit(1);
}

if (isDependabotPr(pr, process.env.GITHUB_ACTOR || '')) {
  console.log('Dependabot PR detected; skipping Codex review by design.');
  process.exit(0);
}

if (isChangesetReleasePr(pr)) {
  console.log('Changeset release PR detected; bypassing Codex review by design.');
  process.exit(0);
}

const repoSlug = process.env.GITHUB_REPOSITORY || '';
const [owner, repo] = repoSlug.split('/');
if (!owner || !repo) {
  console.error('GITHUB_REPOSITORY is not set or invalid.');
  process.exit(1);
}

const githubToken = process.env.GITHUB_TOKEN;
if (!githubToken) {
  console.error('GITHUB_TOKEN is not set.');
  process.exit(1);
}

const openAiKey = process.env.OPENAI_API_KEY;
if (!openAiKey) {
  console.error('OPENAI_API_KEY is not set.');
  process.exit(1);
}

const githubApi = process.env.GITHUB_API_URL || 'https://api.github.com';
const model = process.env.CODEX_REVIEW_MODEL || 'gpt-5.5';
function positiveInteger(name, fallback) {
  const text = process.env[name] ?? String(fallback);
  const value = Number(text);
  if (!/^[1-9]\d*$/.test(text) || !Number.isSafeInteger(value)) {
    throw new Error(`${name} must be a positive integer.`);
  }
  return value;
}
const maxDiffChars = positiveInteger('CODEX_REVIEW_DIFF_MAX', 120000);
const maxOutputTokens = positiveInteger('CODEX_REVIEW_MAX_OUTPUT_TOKENS', 6000);
const reasoningEffort = process.env.CODEX_REVIEW_REASONING_EFFORT || 'low';
if (!pr.head?.sha) throw new Error('PR head SHA is required.');
const codexReviewMarker = `<!-- codex-review:${pr.head.sha} -->`;

async function githubRequest(path, options = {}) {
  const response = await fetch(`${githubApi}${path}`, {
    ...options,
    headers: {
      Authorization: `Bearer ${githubToken}`,
      'User-Agent': 'codex-pr-review',
      ...options.headers,
    },
  });

  if (!response.ok) {
    const text = await response.text();
    throw new Error(`GitHub API ${response.status} ${response.statusText}: ${text}`);
  }

  return response;
}

async function requireCurrentHead() {
  const response = await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}`, {
    headers: {Accept: 'application/vnd.github+json'},
  });
  const latestPr = await response.json();
  if (latestPr.head?.sha !== pr.head.sha) throw new Error('PR head changed during review; no review was posted.');
}
await requireCurrentHead();

function taggedReviewBody(review) {
  const result = Buffer.from(JSON.stringify(review)).toString('base64');
  return `${codexReviewMarker}\n${renderStructuredReviewBody(review)}\n<!-- codex-review-result:v1:${result} -->`;
}

async function previousReviews() {
  const reviews = [];
  for (let page = 1; page <= 100; page++) {
    const response = await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}/reviews?per_page=100&page=${page}`, {
      headers: {Accept: 'application/vnd.github+json'},
    });
    const batch = await response.json();
    if (!Array.isArray(batch)) throw new Error('GitHub returned an invalid review list.');
    reviews.push(...batch);
    if (batch.length < 100) return reviews;
  }
  throw new Error('Cannot establish prior review history; refusing another paid review.');
}

const existingReviews = (await previousReviews()).filter((review) =>
  review.user?.login === 'github-actions[bot]' && review.user?.type === 'Bot'
  && review.commit_id === pr.head.sha && review.body?.startsWith(codexReviewMarker)
);
if (existingReviews.length) {
  for (const previous of existingReviews) {
    const result = previous.body.match(/\n<!-- codex-review-result:v1:([A-Za-z0-9+/=]+) -->$/);
    if (!result || previous.state !== 'COMMENTED') {
      throw new Error('Existing review has no verified disposition; refusing a duplicate paid review.');
    }
    const review = parseOpenAiReview({status: 'completed', output_text: Buffer.from(result[1], 'base64').toString('utf8')});
    if (previous.body !== taggedReviewBody(review)) throw new Error('Existing review body differs from its verified result.');
    if (hasBlockingFindings(review)) throw new Error('Existing Codex review contains blocking findings (Critical/High).');
  }
  await requireCurrentHead();
  console.log('Verified nonblocking Codex review already exists for this exact head; no duplicate request.');
  process.exit(0);
}

const diffResponse = await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}`,
  {
    headers: {
      Accept: 'application/vnd.github.v3.diff',
    },
  });

const diff = await diffResponse.text();

const rubrics = loadSkillRubrics(process.cwd());
const systemPrompt = buildSystemPrompt(rubrics);
const userPrompt = buildUserPrompt(pr, diff, maxDiffChars, false);

const openAiResponse = await fetch('https://api.openai.com/v1/responses', {
  method: 'POST',
  headers: {
    Authorization: `Bearer ${openAiKey}`,
    'Content-Type': 'application/json',
  },
  body: JSON.stringify(buildOpenAiReviewRequestPayload({
    model,
    systemPrompt,
    userPrompt,
    maxOutputTokens,
    reasoningEffort,
  })),
});

if (!openAiResponse.ok) {
  const text = await openAiResponse.text();
  console.error(buildOpenAiErrorDiagnostics({
    status: openAiResponse.status,
    statusText: openAiResponse.statusText,
    headers: openAiResponse.headers,
    bodyText: text,
  }));
  throw new Error(`OpenAI API ${openAiResponse.status} ${openAiResponse.statusText}. See OpenAI error diagnostics above.`);
}

const openAiPayload = await openAiResponse.json();
const review = parseOpenAiReview(openAiPayload);
const reviewBody = renderStructuredReviewBody(review);
console.log(buildOpenAiResponseDiagnostics({
  payload: openAiPayload,
  extractedReviewBody: reviewBody,
  isStructurallyValid: true,
}));

await requireCurrentHead();

const taggedBody = taggedReviewBody(review);

await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}/reviews`, {
  method: 'POST',
  headers: {
    Accept: 'application/vnd.github+json',
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    body: taggedBody,
    event: 'COMMENT',
    commit_id: pr.head.sha,
  }),
});

console.log('Codex review posted.');
if (hasBlockingFindings(review)) throw new Error('Codex review contains blocking findings (Critical/High).');
