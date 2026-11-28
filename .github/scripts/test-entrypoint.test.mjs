import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import test from 'node:test';

test('shared validation runs repository-tool Node tests and propagates their failure', () => {
  const artifacts = resolve('.artifacts'); mkdirSync(artifacts, {recursive: true});
  const root = mkdtempSync(join(artifacts, 'test-entrypoint-'));
  try {
    mkdirSync(join(root, 'scripts')); mkdirSync(join(root, 'bin'));
    copyFileSync(new URL('../../scripts/test.sh', import.meta.url), join(root, 'scripts/test.sh'));
    for (const command of ['git', 'python3', 'sh']) writeFileSync(join(root, 'bin', command), '#!/bin/sh\nexit 0\n', {mode: 0o755});
    writeFileSync(join(root, 'bin/node'), '#!/bin/sh\ncase "$*" in *scripts/tests/*) exit 31;; *) exit 0;; esac\n', {mode: 0o755});
    const result = spawnSync('/bin/sh', ['scripts/test.sh'], {cwd: root, env: {...process.env, PATH: `${join(root, 'bin')}:${process.env.PATH}`}, encoding: 'utf8'});
    assert.equal(result.status, 31, 'the migrated suite must run and its failure must fail shared validation');
  } finally { rmSync(root, {recursive: true, force: true}); }
});
