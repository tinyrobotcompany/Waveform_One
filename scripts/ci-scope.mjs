#!/usr/bin/env node
/** Select validation from the complete base/head diff, including deleted paths. */
import {execFileSync} from 'node:child_process';
import {appendFileSync} from 'node:fs';
import {extname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {parseArgs} from 'node:util';

export const APPS = Object.freeze(['led_test', 'mic_test', 'visualizer', 'kovi_scroll', 'quinie_scroll', 'p4_display']);
const fullValidation = new Set([
  '.github/workflows/validate.yml', '.github/workflows/pr-checks.yml', '.github/workflows/release.yml',
  'scripts/ci-scope.py', 'scripts/ci-scope.mjs', 'scripts/check-idf-security.mjs', 'scripts/setup-idf.sh', 'scripts/idf.sh',
]);
const p4Delivery = new Set(['scripts/build-production.py', 'scripts/build-production.mjs',
  'scripts/package-p4-update.py', 'scripts/package-p4-update.mjs', 'scripts/espsecure.sh']);
const all = () => ({firmware: [...APPS], tests: true});

export function select(paths) {
  const apps = new Set();
  let tests = false;
  for (const path of paths) {
    if (!path) continue;
    if (path === 'docs/release-evidence.md') { tests = true; continue; }
    if (['.md', '.rst', '.txt'].includes(extname(path).toLowerCase()) && !path.endsWith('CMakeLists.txt')) continue;
    if (fullValidation.has(path)) return all();
    if (path.startsWith('firmware/esp32/')) {
      tests = true;
      const app = path.split('/')[2];
      if (APPS.includes(app)) {
        apps.add(app);
        if (app === 'mic_test') apps.add('visualizer');
      } else if (app !== 'tests') return all();
      continue;
    }
    if (path.startsWith('pi/') || path.startsWith('backend/')) { tests = true; continue; }
    if (path.startsWith('scripts/') || path.startsWith('.github/scripts/') || path.startsWith('.githooks/')) {
      tests = true;
      if (p4Delivery.has(path)) apps.add('p4_display');
      if (['scripts/package-firmware.py', 'scripts/package-firmware.mjs'].includes(path)) apps.add('visualizer');
      continue;
    }
    return all();
  }
  return {firmware: APPS.filter(app => apps.has(app)), tests};
}

function main() {
  const {values} = parseArgs({options: {base: {type: 'string'}, head: {type: 'string'}}, allowPositionals: false});
  if (Boolean(values.base) !== Boolean(values.head)) throw new Error('--base and --head must be supplied together');
  const result = values.base ? select(execFileSync('git', [
    'diff', '--name-only', '--no-renames', '-z', `${values.base}...${values.head}`, '--',
  ], {encoding: 'utf8', maxBuffer: 16 * 1024 * 1024}).split('\0')) : all();
  if (!process.env.GITHUB_OUTPUT) throw new Error('GITHUB_OUTPUT is required');
  appendFileSync(process.env.GITHUB_OUTPUT, Object.entries(result).map(([key, value]) => `${key}=${JSON.stringify(value)}\n`).join(''));
  console.log(JSON.stringify(result));
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
}
