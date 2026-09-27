#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Hooks export GIT_DIR/GIT_INDEX_FILE; do not leak them into fixture repositories.
for git_variable in $(git rev-parse --local-env-vars); do
    unset "$git_variable"
done
node --test .github/scripts/*.test.mjs pi/web/*.test.mjs
python3 -m unittest discover -s scripts/tests -v
PYTHONPATH=pi/recognition python3 -m unittest discover -s pi/recognition/tests -v
python3 -m unittest discover -s pi/updater/tests -v
sh firmware/esp32/tests/run.sh
cargo test --manifest-path pi/core/Cargo.toml --locked

if [ "$(uname -s)" = Linux ]; then
    python3 pi/recognition/transport_check.py
fi
