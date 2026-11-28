import {execFileSync, spawnSync} from 'node:child_process';
import {mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync} from 'node:fs';
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';

export const repository = fileURLToPath(new URL('../../', import.meta.url));
export const evidence = '## Recognition accuracy\n\nNot benchmarked.\n\n## Recognition service costs\n\nPrice unverified.\n';
const readme = readFileSync(join(repository, 'README.md'), 'utf8');
export const noticeReadme = readme.slice(readme.indexOf('<!-- privacy-notice:start -->'));
export const cleanEnvironment = Object.fromEntries(Object.entries(process.env).filter(([name]) => !name.startsWith('GIT_')));
export const changeset = (bump = 'patch', notes = 'Fix.') => `---\n"waveform-one": ${bump}\n---\n${notes}\n`;

export function fixture(t) {
  const artifacts = join(repository, '.artifacts');
  mkdirSync(artifacts, {recursive: true});
  const root = mkdtempSync(join(artifacts, 'release-test-'));
  t.after(() => rmSync(root, {recursive: true, force: true}));
  const git = (...args) => execFileSync('git', ['-C', root, ...args], {
    encoding: 'utf8', env: cleanEnvironment, stdio: ['ignore', 'pipe', 'pipe'],
  }).trim();
  const write = (name, content) => {
    const path = join(root, name);
    mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, content);
  };
  const commit = (name, content) => {
    write(name, content);
    git('add', '.');
    git('commit', '-qm', 'test: release fixture');
  };
  git('init', '-q');
  git('config', 'user.name', 'Release Test');
  git('config', 'user.email', 'test@example.com');
  git('config', 'commit.gpgsign', 'false');
  write('docs/release-evidence.md', evidence);
  commit('README.md', noticeReadme);
  const cli = (...args) => spawnSync(process.execPath, [join(root, 'scripts/release.mjs'), ...args], {
    cwd: root, encoding: 'utf8', env: cleanEnvironment,
  });
  return {root, git, write, commit, cli};
}
