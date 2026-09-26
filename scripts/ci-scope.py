"""Select PR validation from the complete base/head diff, including deleted paths."""
import argparse
import json
import os
from pathlib import PurePosixPath
import subprocess

APPS = ['led_test', 'mic_test', 'visualizer', 'kovi_scroll', 'quinie_scroll']


def select(paths):
    apps = set()
    pi = tests = False
    for path in paths:
        if PurePosixPath(path).suffix.lower() in {'.md', '.rst', '.txt'} and not path.endswith('CMakeLists.txt'):
            continue
        if path.startswith(('docs/', '.changeset/')):
            continue
        if path in {'scripts/ci-scope.py', '.github/workflows/validate.yml',
                    '.github/workflows/pr-checks.yml', '.github/workflows/release.yml'}:
            return {'firmware': APPS.copy(), 'pi': True, 'tests': True}
        if path.startswith('firmware/esp32/'):
            tests = True
            app = path.split('/')[2]
            if app in APPS:
                apps.add(app)
                # The visualizer compiles the mic_test audio pipeline and includes its headers.
                if app == 'mic_test':
                    apps.add('visualizer')
            elif app != 'tests':
                apps.update(APPS)
        if path.startswith('pi/'):
            pi = tests = True
        if path.startswith(('scripts/', '.github/scripts/', '.githooks/')):
            tests = True
        if path == 'scripts/package-firmware.py':
            apps.add('visualizer')
    return {'firmware': [app for app in APPS if app in apps], 'pi': pi, 'tests': tests}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base')
    parser.add_argument('--head')
    args = parser.parse_args()
    if args.base and args.head:
        changed = subprocess.check_output([
            'git', 'diff', '--name-only', '--no-renames', '-z', f'{args.base}...{args.head}'])
        result = select(changed.decode('utf-8', errors='surrogateescape').split('\0'))
    elif args.base or args.head:
        parser.error('--base and --head must be supplied together')
    else:
        result = {'firmware': APPS.copy(), 'pi': True, 'tests': True}
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        for key, value in result.items():
            output.write(f'{key}={json.dumps(value)}\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
