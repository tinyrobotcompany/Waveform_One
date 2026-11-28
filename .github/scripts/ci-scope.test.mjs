import assert from 'node:assert/strict';
import {execFileSync, spawnSync} from 'node:child_process';
import {mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import test from 'node:test';
import {APPS, select} from '../../scripts/ci-scope.mjs';

const all = {firmware: [...APPS], tests: true};
test('documentation and changesets skip firmware builds', () => {
  assert.deepEqual(select(['README.md', 'firmware/esp32/visualizer/README.md', '.changeset/example.md']), {firmware: [], tests: false});
  assert.deepEqual(select(['docs/release-evidence.md']), {firmware: [], tests: true});
});
for (const app of APPS) test(`${app} selects its consumers`, () => {
  assert.deepEqual(select([`firmware/esp32/${app}/main/main.cpp`]), {
    firmware: APPS.filter(candidate => candidate === app || app === 'mic_test' && candidate === 'visualizer'), tests: true,
  });
});
test('audio headers and app configuration select their consumers', () => {
  assert.deepEqual(select(['firmware/esp32/mic_test/main/audio_pipeline.h']).firmware, ['mic_test', 'visualizer']);
  for (const name of ['CMakeLists.txt', 'sdkconfig.defaults', 'dependencies.lock', 'main/idf_component.yml']) {
    assert.deepEqual(select([`firmware/esp32/visualizer/${name}`]).firmware, ['visualizer']);
  }
});
test('shared firmware and validation changes select all builds', () => {
  for (const path of ['firmware/esp32/components/common/code.c', '.github/workflows/validate.yml', '.github/workflows/pr-checks.yml', '.github/workflows/release.yml', 'scripts/ci-scope.py', 'scripts/ci-scope.mjs', 'scripts/check-idf-security.mjs', 'scripts/setup-idf.sh', 'scripts/idf.sh']) {
    assert.deepEqual(select([path]), all, path);
  }
});
test('Pi deletion, backend and ordinary automation select host tests', () => {
  for (const path of ['pi/core/src/lib.rs', 'pi/core/Cargo.lock', 'backend/src/Example.cs', 'backend/global.json', '.github/scripts/codex-pr-review.mjs']) {
    assert.deepEqual(select([path]), {firmware: [], tests: true}, path);
  }
});
test('both migration-era packagers and builders select production artifacts', () => {
  for (const suffix of ['py', 'mjs']) {
    for (const name of ['build-production', 'package-p4-update']) assert.deepEqual(select([`scripts/${name}.${suffix}`]).firmware, ['p4_display']);
    assert.deepEqual(select([`scripts/package-firmware.${suffix}`]).firmware, ['visualizer']);
  }
  assert.deepEqual(select(['scripts/espsecure.sh']).firmware, ['p4_display']);
});
test('unclassified changes select conservatively, including executable files under docs', () => {
  for (const path of ['new-component/service.cs', '.github/workflows/new.yml', 'docs/helper.mjs', 'tools/new-tool.mjs']) assert.deepEqual(select([path]), all, path);
});
test('empty diff selects no firmware and multiple apps retain stable order', () => {
  assert.deepEqual(select(['']), {firmware: [], tests: false});
  assert.deepEqual(select(['firmware/esp32/p4_display/main/a.cpp', 'firmware/esp32/led_test/main/a.c', 'firmware/esp32/p4_display/main/a.cpp']).firmware, ['led_test', 'p4_display']);
});
test('CLI uses complete git diff including deletions and rejects partial arguments', () => {
  const artifacts = resolve('.artifacts'); mkdirSync(artifacts, {recursive: true});
  const root = mkdtempSync(join(artifacts, 'scope-'));
  const cli = fileURLToPath(new URL('../../scripts/ci-scope.mjs', import.meta.url));
  const env = {...process.env, GITHUB_OUTPUT: join(root, 'output')};
  for (const name of execFileSync('git', ['rev-parse', '--local-env-vars'], {encoding: 'utf8'}).trim().split('\n')) delete env[name];
  const git = (...args) => execFileSync('git', args, {cwd: root, env, encoding: 'utf8'}).trim();
  try {
    git('init', '-q'); git('config', 'user.email', 'fixture@example.invalid'); git('config', 'user.name', 'Fixture'); git('config', 'commit.gpgsign', 'false');
    mkdirSync(join(root, 'backend')); writeFileSync(join(root, 'backend', 'Example.cs'), '// fixture');
    git('add', '.'); git('commit', '-qm', 'fixture'); const base = git('rev-parse', 'HEAD');
    git('rm', '-q', 'backend/Example.cs'); git('commit', '-qm', 'deletion');
    const run = (...args) => spawnSync(process.execPath, [cli, ...args], {cwd: root, env, encoding: 'utf8'});
    assert.equal(run('--base', base, '--head', 'HEAD').status, 0);
    assert.equal(readFileSync(env.GITHUB_OUTPUT, 'utf8'), 'firmware=[]\ntests=true\n');
    assert.notEqual(run('--base', base).status, 0);
    assert.notEqual(run('--base', 'missing-reference', '--head', 'HEAD').status, 0);
    assert.notEqual(run('--unexpected').status, 0);
    assert.equal(run().status, 0);
    assert.ok(readFileSync(env.GITHUB_OUTPUT, 'utf8').endsWith(`firmware=${JSON.stringify(APPS)}\ntests=true\n`));
  } finally { rmSync(root, {recursive: true, force: true}); }
});
test('validation workflow invokes Node selector with a pinned Node setup', () => {
  const workflow = readFileSync(new URL('../workflows/validate.yml', import.meta.url), 'utf8');
  const scopeJob = workflow.split('\n  tests:')[0];
  assert.match(scopeJob, /actions\/setup-node@[a-f0-9]{40}/);
  assert.match(scopeJob, /node scripts\/ci-scope\.mjs --base/);
  assert.doesNotMatch(scopeJob, /python3 scripts\/ci-scope\.py/);
});
