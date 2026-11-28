import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import test from 'node:test';

test('shared validation executes backend validation and propagates failure', () => {
  const artifacts = resolve('backend/.artifacts'); mkdirSync(artifacts, {recursive: true});
  const root = mkdtempSync(join(artifacts, 'entrypoint-'));
  try {
    mkdirSync(join(root, 'scripts')); mkdirSync(join(root, 'backend')); mkdirSync(join(root, 'bin'));
    copyFileSync(new URL('../../scripts/test.sh', import.meta.url), join(root, 'scripts/test.sh'));
    writeFileSync(join(root, 'scripts/test-backend.sh'), '#!/bin/sh\nexit 31\n');
    for (const name of ['git', 'node', 'python3']) writeFileSync(join(root, 'bin', name), '#!/bin/sh\nexit 0\n', {mode: 0o755});
    writeFileSync(join(root, 'bin/sh'), '#!/bin/sh\ncase "$1" in *test-backend.sh) exec /bin/sh "$@";; *) exit 0;; esac\n', {mode: 0o755});
    const result = spawnSync('/bin/sh', ['scripts/test.sh'], {cwd: root, env: {...process.env, PATH: `${join(root, 'bin')}:${process.env.PATH}`}, encoding: 'utf8'});
    assert.equal(result.status, 31);
  } finally { rmSync(root, {recursive: true, force: true}); }
});
test('backend validation locks restore, records coverage and rejects empty discovery', () => {
  const script = readFileSync(new URL('../../scripts/test-backend.sh', import.meta.url), 'utf8');
  assert.match(script, /restore Waveform\.Certificates\.slnx --locked-mode/);
  assert.match(script, /test Waveform\.Certificates\.slnx --no-restore/);
  assert.match(script, /RunConfiguration\.TreatNoTestsAsError=true/);
  assert.match(script, /XPlat Code Coverage/);
});
test('host-test CI installs the backend-selected SDK before validation', () => {
  const workflow = readFileSync(new URL('../workflows/validate.yml', import.meta.url), 'utf8');
  const host = workflow.split('\n  tests:')[1].split('\n  firmware:')[0];
  assert.match(host, /actions\/setup-dotnet@[a-f0-9]{40}/);
  assert.match(host, /global-json-file: backend\/global.json/);
  assert.ok(host.indexOf('setup-dotnet@') < host.indexOf('sh scripts/test.sh'));
});
