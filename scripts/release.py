#!/usr/bin/env python3
"""Immutable changesets and tag-based releases; no dependency installation required."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def parse_changeset(text):
    match = re.fullmatch(r'---\s*\n"waveform-one": (patch|minor|major)\s*\n---\s*\n(.+)',
                         text.strip(), re.DOTALL)
    if not match or not match[2].strip():
        raise ValueError('Changeset must contain "waveform-one": patch|minor|major and release notes')
    return match[1], match[2].strip()


def bump(version, bumps):
    major, minor, patch = map(int, version.split('.'))
    if 'major' in bumps:
        return f'{major + 1}.0.0'
    if 'minor' in bumps:
        return f'{major}.{minor + 1}.0'
    return f'{major}.{minor}.{patch + 1}'


def parse_release_evidence(text):
    headings = ['Recognition accuracy', 'Recognition service costs']
    if re.findall(r'^## (.+)$', text, re.MULTILINE) != headings:
        raise ValueError('Release evidence must contain accuracy and service-cost sections exactly once')
    sections = re.split(r'^## .+\n', text + '\n', flags=re.MULTILINE)
    if sections[0].strip() or any(not re.sub(r'<!--.*?-->', '', section, flags=re.DOTALL).strip()
                                  for section in sections[1:]):
        raise ValueError('Release evidence sections must not be empty')
    return text.strip()


def release_evidence(root, head):
    """Use committed disclosures, never uncommitted working-tree claims."""
    return parse_release_evidence(git(root, 'show', f'{head}:docs/release-evidence.md'))


def is_changeset(path):
    return path.startswith('.changeset/') and path.endswith('.md') and path != '.changeset/README.md'


def new_changesets(root, base, head='HEAD'):
    if base:
        changes = git(root, 'diff', '--no-renames', '--name-status', base, head).splitlines()
        paths = []
        for change in changes:
            status, path = change.split('\t', 1)
            if is_changeset(path):
                if status != 'A':
                    raise ValueError(f'Changesets are immutable; add a new file instead: {path}')
                paths.append(path)
    else:
        paths = [p for p in git(root, 'ls-tree', '-r', '--name-only', head).splitlines() if is_changeset(p)]
    return [parse_changeset(git(root, 'show', f'{head}:{path}')) for path in paths]


def check(root, base):
    release_evidence(root, 'HEAD')
    base = git(root, 'merge-base', base, 'HEAD')
    entries = new_changesets(root, base)
    paths = git(root, 'diff', '--name-only', base, 'HEAD').splitlines()
    needs_notes = any(p.startswith(('firmware/', 'pi/', 'protocol/', 'scripts/', '.github/', '.githooks/'))
                     and not p.endswith('.md') for p in paths)
    if needs_notes and not entries:
        raise ValueError('Add a new .changeset/*.md for firmware, Pi, protocol or automation changes')


def plan(root):
    tags = [t for t in git(root, 'tag', '--merged', 'HEAD').splitlines()
            if re.fullmatch(r'v\d+\.\d+\.\d+', t)]
    tags.sort(key=lambda t: tuple(map(int, t[1:].split('.'))))
    latest = tags[-1] if tags else None
    head = git(root, 'rev-parse', 'HEAD')
    rerun = latest and git(root, 'rev-list', '-n', '1', latest) == head
    base = (tags[-2] if len(tags) > 1 else None) if rerun else latest
    entries = new_changesets(root, base)
    if not entries:
        return None
    tag = latest if rerun else 'v' + bump(base[1:] if base else '0.0.0', [e[0] for e in entries])
    notes = f'# Waveform One {tag}\n\n' + '\n\n'.join(e[1] for e in entries)
    notes += '\n\n' + release_evidence(root, head)
    notes += f'\n\nCommit: `{head}`\n\nSigned P4 application updates require the one-time A/B USB bootstrap. Pi/S3 update assets remain available for commissioned legacy devices.\n'
    return {'tag': tag, 'commit': head, 'notes': notes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('check').add_argument('base')
    sub.add_parser('plan').add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.command == 'check':
        check(root, args.base)
    else:
        args.output.write_text(json.dumps(plan(root), indent=2) + '\n')


if __name__ == '__main__':
    main()
