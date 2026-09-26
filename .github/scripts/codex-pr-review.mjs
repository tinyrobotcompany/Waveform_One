#!/usr/bin/env node

import fs from 'fs';
import process from 'process';
import {
  buildSystemPrompt,
  buildUserPrompt,
  buildOpenAiErrorDiagnostics,
  buildOpenAiReviewRequestPayload,
  buildOpenAiResponseDiagnostics,
  extractReviewBodyFromOpenAiPayload,
  isStructurallyValidReviewBody,
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
const maxDiffChars = Number.parseInt(process.env.CODEX_REVIEW_DIFF_MAX || '120000', 10);
const maxOutputTokens = Number.parseInt(process.env.CODEX_REVIEW_MAX_OUTPUT_TOKENS || '6000', 10);
const reasoningEffort = process.env.CODEX_REVIEW_REASONING_EFFORT || 'medium';
const codexReviewMarker = `<!-- codex-review:${pr.head?.sha || process.env.GITHUB_SHA || 'unknown'} -->`;

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

const existingReviewsResponse = await githubRequest(
  `/repos/${owner}/${repo}/pulls/${pr.number}/reviews?per_page=100`,
  {
    headers: {
      Accept: 'application/vnd.github+json',
    },
  }
);
const existingReviews = await existingReviewsResponse.json();
const alreadyReviewed = Array.isArray(existingReviews)
  && existingReviews.some((review) => typeof review?.body === 'string' && review.body.includes(codexReviewMarker));

if (alreadyReviewed) {
  console.log('Existing Codex review found for this PR; skipping duplicate review comment.');
  process.exit(0);
}

const diffResponse = await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}`,
  {
    headers: {
      Accept: 'application/vnd.github.v3.diff',
    },
  });

let diff = await diffResponse.text();
let diffTruncated = false;
if (diff.length > maxDiffChars) {
  diff = diff.slice(0, maxDiffChars);
  diffTruncated = true;
}

const rubrics = loadSkillRubrics(process.cwd());
const systemPrompt = buildSystemPrompt(rubrics);
const userPrompt = buildUserPrompt(pr, diff, maxDiffChars, diffTruncated);

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
const extractedReviewBody = extractReviewBodyFromOpenAiPayload(openAiPayload);
const structurallyValidReviewBody = isStructurallyValidReviewBody(extractedReviewBody);
console.log(buildOpenAiResponseDiagnostics({
  payload: openAiPayload,
  extractedReviewBody,
  isStructurallyValid: structurallyValidReviewBody,
}));

if (openAiPayload.status !== 'completed' || !extractedReviewBody || !structurallyValidReviewBody) {
  throw new Error('OpenAI did not return a complete, valid review; no review was posted.');
}
const reviewBody = extractedReviewBody;
const latestPrResponse = await githubRequest(`/repos/${owner}/${repo}/pulls/${pr.number}`);
const latestPr = await latestPrResponse.json();
if (latestPr.head.sha !== pr.head.sha) {
  throw new Error('PR changed during review; the newer workflow must review the new commit.');
}

const taggedBody = `${codexReviewMarker}\n${reviewBody}`;

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
