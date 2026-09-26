import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

for (const existing of [false, true]) {
  test(`ruleset ${existing ? 'update preserves existing checks' : 'creation requires Codex Review'}`, () => {
    const temp = mkdtempSync(join(tmpdir(), 'waveform-ruleset-'));
    try {
      const loader = join(temp, 'fake.mjs');
      writeFileSync(loader, `
        globalThis.fetch = async (url, options = {}) => {
          if (options.method) {
            console.log('SAVED=' + options.body);
            return Response.json({});
          }
          if (url.includes('?')) return Response.json(${existing ? '[{id: 1, name: "Require Codex Review"}]' : '[]'});
          return Response.json({name: 'Require Codex Review', target: 'branch', enforcement: 'active',
            conditions: {ref_name: {include: ['refs/heads/main'], exclude: []}},
            rules: [{type: 'required_status_checks', parameters: {
              required_status_checks: [{context: 'Existing Tests'}], strict_required_status_checks_policy: true}}]});
        };
      `);
      const result = spawnSync(process.execPath,
        ['--import', loader, '.github/scripts/ensure-codex-ruleset.mjs'], {
          encoding: 'utf8', env: {...process.env, REPO_ADMIN_TOKEN: 'fake', GITHUB_REPOSITORY: 'test/test'},
        });
      assert.equal(result.status, 0, result.stderr);
      const payload = JSON.parse(result.stdout.split('\n').find(line => line.startsWith('SAVED=')).slice(6));
      assert.deepEqual(payload.conditions.ref_name.include, ['refs/heads/main']);
      const checks = payload.rules.find(rule => rule.type === 'required_status_checks').parameters;
      assert.ok(checks.required_status_checks.some(check => check.context === 'Codex Review'));
      if (existing) {
        assert.ok(checks.required_status_checks.some(check => check.context === 'Existing Tests'));
        assert.equal(checks.strict_required_status_checks_policy, true);
      }
    } finally {
      rmSync(temp, {recursive: true, force: true});
    }
  });
}
