#!/usr/bin/env node

import process from 'process';

const token = process.env.REPO_ADMIN_TOKEN || '';
if (!token) {
  console.error('REPO_ADMIN_TOKEN is required to manage repository rulesets.');
  process.exit(1);
}

const repoSlug = process.env.GITHUB_REPOSITORY || '';
const [owner, repo] = repoSlug.split('/');
if (!owner || !repo) {
  console.error('GITHUB_REPOSITORY is not set or invalid.');
  process.exit(1);
}

const githubApi = process.env.GITHUB_API_URL || 'https://api.github.com';
const rulesetName = process.env.CODEX_RULESET_NAME || 'Require Codex Review';
const defaultRequiredCheck = process.env.CODEX_REVIEW_CHECK || 'Codex Review';
const targetBranch = process.env.CODEX_RULESET_BRANCH || 'refs/heads/main';

function parseRepositoryRoleIds() {
  const rawList = process.env.CODEX_BYPASS_REPOSITORY_ROLE_IDS || '';
  if (rawList.trim()) {
    return [...new Set(
      rawList
        .split(',')
        .map((value) => Number.parseInt(value.trim(), 10))
        .filter((value) => Number.isInteger(value) && value > 0),
    )];
  }

  const legacy = Number.parseInt(process.env.CODEX_BYPASS_REPOSITORY_ROLE_ID || '2', 10);
  if (Number.isInteger(legacy) && legacy > 0) {
    return [legacy];
  }

  return [2];
}

const bypassRepositoryRoleIds = parseRepositoryRoleIds();
const bypassIntegrationId = Number.parseInt(
  process.env.CODEX_BYPASS_INTEGRATION_ID || '15368',
  10,
);
const requiredChecks = resolveRequiredChecks();
const canUseIntegrationBypass = Number.isInteger(bypassIntegrationId) && bypassIntegrationId > 0;

function resolveRequiredChecks() {
  const csv = process.env.CODEX_REQUIRED_CHECKS || '';
  const checks = csv
    .split(',')
    .map((entry) => entry.trim())
    .filter(Boolean);

  if (checks.length > 0) {
    return Array.from(new Set(checks));
  }

  return [defaultRequiredCheck];
}

function ensureBypassActors(existingBypassActors = [], options = {}) {
  const { includeIntegration = true } = options;
  const actors = Array.isArray(existingBypassActors)
    ? existingBypassActors.filter((actor) => includeIntegration || actor?.actor_type !== 'Integration')
    : [];

  for (const roleId of bypassRepositoryRoleIds) {
    const hasRepositoryRoleBypass = actors.some(
      (actor) =>
        actor?.actor_type === 'RepositoryRole' &&
        Number(actor?.actor_id) === roleId &&
        actor?.bypass_mode === 'always',
    );

    if (!hasRepositoryRoleBypass) {
      actors.push({
        actor_id: roleId,
        actor_type: 'RepositoryRole',
        bypass_mode: 'always',
      });
    }
  }

  if (includeIntegration && canUseIntegrationBypass) {
    const hasIntegrationBypass = actors.some(
      (actor) =>
        actor?.actor_type === 'Integration' &&
        Number(actor?.actor_id) === bypassIntegrationId &&
        actor?.bypass_mode === 'always',
    );

    if (!hasIntegrationBypass) {
      actors.push({
        actor_id: bypassIntegrationId,
        actor_type: 'Integration',
        bypass_mode: 'always',
      });
    }
  }

  return actors;
}

class GitHubApiError extends Error {
  constructor(status, statusText, body) {
    super(`GitHub API ${status} ${statusText}: ${body}`);
    this.name = 'GitHubApiError';
    this.status = status;
    this.statusText = statusText;
    this.body = body;
  }
}

async function githubRequest(path, options = {}) {
  const response = await fetch(`${githubApi}${path}`, {
    ...options,
    headers: {
      Authorization: `Bearer ${token}`,
      'User-Agent': 'codex-ruleset-automation',
      Accept: 'application/vnd.github+json',
      ...options.headers,
    },
  });

  if (!response.ok) {
    const text = await response.text();
    throw new GitHubApiError(response.status, response.statusText, text);
  }

  return response;
}

function shouldRetryWithoutIntegration(error) {
  if (!(error instanceof GitHubApiError)) {
    return false;
  }

  if (error.status !== 400 && error.status !== 422) {
    return false;
  }

  const body = (error.body || '').toLowerCase();
  return body.includes('integration') || body.includes('actor_type');
}

async function saveRulesetWithIntegrationFallback(method, path, payloadFactory) {
  const withIntegration = payloadFactory(true);

  try {
    await githubRequest(path, {
      method,
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(withIntegration),
    });
    return true;
  } catch (error) {
    if (!canUseIntegrationBypass || !shouldRetryWithoutIntegration(error)) {
      throw error;
    }
  }

  const withoutIntegration = payloadFactory(false);
  console.warn(
    'GitHub rejected Integration bypass actor while updating ruleset. Retrying with RepositoryRole bypass only.',
  );

  await githubRequest(path, {
    method,
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(withoutIntegration),
  });

  return false;
}

function ensureRequiredCheckRule(ruleset) {
  const existingRules = Array.isArray(ruleset.rules) ? ruleset.rules : [];
  const targetRule = existingRules.find((rule) => rule.type === 'required_status_checks');

  if (!targetRule) {
    return [
      ...existingRules,
      {
        type: 'required_status_checks',
        parameters: {
          required_status_checks: requiredChecks.map((context) => ({ context })),
          strict_required_status_checks_policy: false,
        },
      },
    ];
  }

  const existingChecks = Array.isArray(targetRule.parameters?.required_status_checks)
    ? targetRule.parameters.required_status_checks
    : [];

  const existingContexts = new Set(existingChecks.map((check) => check.context).filter(Boolean));
  const missingChecks = requiredChecks.filter((context) => !existingContexts.has(context));

  if (missingChecks.length === 0) {
    return existingRules;
  }

  const updatedRule = {
    ...targetRule,
    parameters: {
      ...targetRule.parameters,
      required_status_checks: [
        ...existingChecks,
        ...missingChecks.map((context) => ({ context })),
      ],
      strict_required_status_checks_policy: targetRule.parameters?.strict_required_status_checks_policy ?? false,
    },
  };

  return existingRules.map((rule) => (rule.type === 'required_status_checks' ? updatedRule : rule));
}

const rulesetResponse = await githubRequest(`/repos/${owner}/${repo}/rulesets?per_page=100`);
const rulesets = await rulesetResponse.json();
const existing = Array.isArray(rulesets)
  ? rulesets.find((ruleset) => ruleset.name === rulesetName)
  : null;

if (!existing) {
  const payloadFactory = (includeIntegration) => ({
    name: rulesetName,
    target: 'branch',
    enforcement: 'active',
    conditions: {
      ref_name: {
        include: [targetBranch],
        exclude: [],
      },
    },
    rules: [
      {
        type: 'required_status_checks',
        parameters: {
          required_status_checks: requiredChecks.map((context) => ({ context })),
          strict_required_status_checks_policy: false,
        },
      },
    ],
    bypass_actors: ensureBypassActors([], { includeIntegration }),
  });

  const usedIntegrationBypass = await saveRulesetWithIntegrationFallback(
    'POST',
    `/repos/${owner}/${repo}/rulesets`,
    payloadFactory,
  );

  const bypassMode = usedIntegrationBypass ? 'RepositoryRole + Integration' : 'RepositoryRole only';
  console.log(
    `Created ruleset "${rulesetName}" with required checks: ${requiredChecks.join(', ')}. Bypass mode: ${bypassMode}.`,
  );
  process.exit(0);
}

const rulesetDetailsResponse = await githubRequest(`/repos/${owner}/${repo}/rulesets/${existing.id}`);
const rulesetDetails = await rulesetDetailsResponse.json();

const updatedRules = ensureRequiredCheckRule(rulesetDetails);
const payloadFactory = (includeIntegration) => ({
  name: rulesetDetails.name,
  target: rulesetDetails.target,
  enforcement: rulesetDetails.enforcement,
  conditions: rulesetDetails.conditions,
  rules: updatedRules,
  bypass_actors: ensureBypassActors(rulesetDetails.bypass_actors || [], { includeIntegration }),
});

const usedIntegrationBypass = await saveRulesetWithIntegrationFallback(
  'PUT',
  `/repos/${owner}/${repo}/rulesets/${existing.id}`,
  payloadFactory,
);

const bypassMode = usedIntegrationBypass ? 'RepositoryRole + Integration' : 'RepositoryRole only';
console.log(
  `Updated ruleset "${rulesetName}" to require checks: ${requiredChecks.join(', ')}. Bypass mode: ${bypassMode}.`,
);
