#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Hooks export GIT_DIR/GIT_INDEX_FILE; do not leak them into fixture repositories.
for git_variable in $(git rev-parse --local-env-vars); do
    unset "$git_variable"
done
node --test .github/scripts/*.test.mjs
sh scripts/test-backend.sh
python3 -m unittest discover -s scripts/tests -v
python3 -m unittest discover -s tools/music_benchmark -p 'test_*.py' -v
python3 -m unittest discover -s tools/history_bridge -p 'test_*.py' -v
sh firmware/esp32/tests/run.sh

python3 -m unittest discover -s tools/product_validation -p 'test_*.py' -v
