#!/usr/bin/env node

/** Immutable changesets and tag-based releases using only the Node standard library. */
import {execFileSync} from 'node:child_process';
import {writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
export function parseChangeset(text) {
  const match = /^---\s*\n"waveform-one": (patch|minor|major)\s*\n---\s*\n(.+)$/s.exec(text.trim());
  if (!match || !match[2].trim()) {
    throw new Error('Changeset must contain "waveform-one": patch|minor|major and release notes');
  }
  return [match[1], match[2].trim()];
}

function versionParts(version) {
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error(`Invalid release version: ${version}`);
  return version.split('.').map(BigInt);
}

export function bump(version, bumps) {
  const [major, minor, patch] = versionParts(version);
  if (bumps.includes('major')) return `${major + 1n}.0.0`;
  if (bumps.includes('minor')) return `${major}.${minor + 1n}.0`;
  return `${major}.${minor}.${patch + 1n}`;
}

export function parseReleaseEvidence(text) {
  const headings = [...text.matchAll(/^## (.+)$/gm)].map((match) => match[1]);
  if (headings.length !== 2 || headings[0] !== 'Recognition accuracy'
    || headings[1] !== 'Recognition service costs') {
    throw new Error('Release evidence must contain accuracy and service-cost sections exactly once');
  }
  const sections = `${text}\n`.split(/^## .+\n/gm);
  if (sections[0].trim() || sections.slice(1).some((section) => !section.replace(/<!--.*?-->/gs, '').trim())) {
    throw new Error('Release evidence sections must not be empty');
  }
  return text.trim();
}

function git(root, ...args) {
  return execFileSync('git', ['-C', root, ...args], {
    encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'],
  }).replace(/\r\n?/g, '\n').trim();
}

const isChangeset = path => path.startsWith('.changeset/') && path.endsWith('.md') && path !== '.changeset/README.md';

export function newChangesets(root, base, head = 'HEAD') {
  let paths;
  if (base) {
    const fields = git(root, 'diff', '--no-renames', '--name-status', '-z', base, head).split('\0');
    paths = [];
    for (let i = 0; i + 1 < fields.length; i += 2) {
      const [status, path] = fields.slice(i, i + 2);
      if (!isChangeset(path)) continue;
      if (status !== 'A') throw new Error(`Changesets are immutable; add a new file instead: ${path}`);
      paths.push(path);
    }
  } else {
    paths = git(root, 'ls-tree', '-r', '--name-only', '-z', head).split('\0').filter(isChangeset);
  }
  return paths.map(path => parseChangeset(git(root, 'show', `${head}:${path}`)));
}

export function releaseEvidence(root, head) {
  return parseReleaseEvidence(git(root, 'show', `${head}:docs/release-evidence.md`));
}

export function check(root, base) {
  releaseEvidence(root, 'HEAD');
  const ancestor = git(root, 'merge-base', base, 'HEAD');
  const entries = newChangesets(root, ancestor);
  const paths = git(root, 'diff', '--name-only', '-z', ancestor, 'HEAD').split('\0');
  const needsNotes = paths.some(path => /^(firmware|backend|pi|protocol|scripts|\.github|\.githooks)\//.test(path) && !path.endsWith('.md'));
  if (needsNotes && !entries.length) throw new Error('Add a new .changeset/*.md for firmware, backend, Pi, protocol or automation changes');
}

export function privacyNotice(root, head) {
  const readme = git(root, 'show', `${head}:README.md`);
  const start = '<!-- privacy-notice:start -->';
  const end = '<!-- privacy-notice:end -->';
  const match = /<!-- privacy-notice:start -->\s*(.+?)\s*<!-- privacy-notice:end -->/s.exec(readme);
  if (readme.split(start).length !== 2 || readme.split(end).length !== 2 || !match || !match[1].trim()) {
    throw new Error('README must contain the microphone privacy notice for every release');
  }
  return match[1].trim();
}

function compareTags(a, b) {
  const left = versionParts(a.slice(1));
  const right = versionParts(b.slice(1));
  for (let i = 0; i < left.length; i++) {
    if (left[i] !== right[i]) return left[i] < right[i] ? -1 : 1;
  }
  return 0;
}

export function plan(root) {
  const tags = git(root, 'tag', '--merged', 'HEAD').split('\n').filter(tag => /^v\d+\.\d+\.\d+$/.test(tag)).sort(compareTags);
  const latest = tags.at(-1);
  const head = git(root, 'rev-parse', 'HEAD');
  const rerun = latest && git(root, 'rev-list', '-n', '1', latest) === head;
  const base = rerun ? tags.at(-2) : latest;
  const entries = newChangesets(root, base);
  if (!entries.length) return null;
  const tag = rerun ? latest : 'v' + bump(base ? base.slice(1) : '0.0.0', entries.map(entry => entry[0]));
  let notes = `# Waveform One ${tag}\n\n` + entries.map(entry => entry[1]).join('\n\n');
  notes += '\n\n' + releaseEvidence(root, head);
  notes += `\n\nCommit: \`${head}\`\n\nSigned P4 application updates require the one-time A/B USB bootstrap. Pi/S3 update assets remain available for commissioned legacy devices.\n`;
  notes += '\n' + privacyNotice(root, head) + '\n';
  return {tag, commit: head, notes};
}

function main(args) {
  const root = fileURLToPath(new URL('../', import.meta.url));
  if (args.length === 2 && args[0] === 'check') {
    check(root, args[1]);
  } else if (args.length === 3 && args[0] === 'plan' && args[1] === '--output') {
    writeFileSync(args[2], JSON.stringify(plan(root), null, 2) + '\n');
  } else {
    throw new Error('Usage: node scripts/release.mjs check <base> | plan --output <path>');
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    main(process.argv.slice(2));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
