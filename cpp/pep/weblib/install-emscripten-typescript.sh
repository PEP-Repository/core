#!/usr/bin/env sh
# Used from CMake
# tsc isn't installed by default as it's in EMSDK devDependencies but we need it for --emit-tsd,
# see related https://github.com/emscripten-core/emsdk/issues/1370
set -eu

EMSCRIPTEN="${EMSCRIPTEN:-$EMSDK/upstream/emscripten/}"
cd -- "$EMSCRIPTEN"
if typescript_version="$(jq --raw-output --exit-status .devDependencies.typescript ./package.json)"; then
  # Make sure to specify the version: otherwise it will install the latest version
  npm install --no-audit --omit=dev --save-prod "typescript@${typescript_version}"
fi
