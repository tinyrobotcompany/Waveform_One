#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
git config --local core.hooksPath .githooks
printf '%s\n' 'Installed pre-commit hook: all host unit tests run before commits.'
