#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd "$(dirname "$0")/.." && pwd)
export IDF_PATH="${IDF_PATH:-$project_root/.artifacts/esp-idf}"
export PIP_CACHE_DIR="${PIP_CACHE_DIR:-$project_root/.artifacts/pip-cache}"
export IDF_COMPONENT_CACHE_PATH="${IDF_COMPONENT_CACHE_PATH:-$project_root/.artifacts/component-cache}"
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$project_root/.artifacts/idf-tools}"
node "$project_root/scripts/check-idf-security.mjs" "$IDF_PATH"
# Verify before evaluating SDK code; preserve the caller's app directory and args.
source "$IDF_PATH/export.sh" >/dev/null
exec python "$IDF_PATH/tools/idf.py" "$@"
