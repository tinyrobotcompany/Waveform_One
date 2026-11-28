#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd "$(dirname "$0")/.." && pwd)
export IDF_PATH="${IDF_PATH:-$project_root/.artifacts/esp-idf}"
export PIP_CACHE_DIR="${PIP_CACHE_DIR:-$project_root/.artifacts/pip-cache}"
export IDF_COMPONENT_CACHE_PATH="${IDF_COMPONENT_CACHE_PATH:-$project_root/.artifacts/component-cache}"
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$project_root/.artifacts/idf-tools}"
export PIP_CACHE_DIR="${PIP_CACHE_DIR:-$project_root/.artifacts/pip-cache}"
sdk_commit=$(node --input-type=module -e 'import {pathToFileURL} from "node:url"; const {SDK_COMMIT}=await import(pathToFileURL(process.argv[2])); console.log(SDK_COMMIT)' setup "$project_root/scripts/check-idf-security.mjs")
if [ ! -e "$IDF_PATH" ]; then
  mkdir -p "$IDF_PATH"
  git -C "$IDF_PATH" init -q
  git -C "$IDF_PATH" remote add origin https://github.com/espressif/esp-idf.git
  git -C "$IDF_PATH" fetch --depth 1 origin "$sdk_commit"
  git -C "$IDF_PATH" checkout --detach FETCH_HEAD
  git -C "$IDF_PATH" submodule update --init --recursive --depth 1
fi
# Never reset or repair an existing checkout silently.
node "$project_root/scripts/check-idf-security.mjs" "$IDF_PATH"
bash "$IDF_PATH/install.sh" esp32s3,esp32p4
