#!/usr/bin/env sh
set -eu

# Start Nginx in the background, serving pep-sample-client
# If already running, just updates configuration

scriptdir="$(realpath -- "$(dirname -- "$0")")"

wasm_build_folder="$(realpath -- "${1:-"$scriptdir/../build/wasm32/Debug/"}")"

cd "$scriptdir"

if [ -z "${DEFAULT_MIME_TYPES-}" ]; then
  if [ -f /etc/nginx/mime.types ]; then
    DEFAULT_MIME_TYPES=/etc/nginx/mime.types
  elif stat -t "${LOCALAPPDATA-/NOPE}/Microsoft/WinGet/Packages"/*nginx*/nginx*/conf/mime.types >/dev/null 2>&1; then
    DEFAULT_MIME_TYPES="$(echo "${LOCALAPPDATA}/Microsoft/WinGet/Packages"/*nginx*/nginx*/conf/mime.types)"
  else
    >&2 echo "Could not deduce default Nginx conf directory with 'mime.types'. Please set envvar DEFAULT_MIME_TYPES to this file."
    exit 1
  fi
fi
cp "$DEFAULT_MIME_TYPES" ../temp/mime.types

cat >../temp/nginx-variables.conf <<EOF
set \$home "$HOME";
set \$emsdk "${EMSDK:?EMSDK envvar not set, please source emsdk_env in your shell}";
set \$build_folder "$wasm_build_folder";
EOF

# `-e stderr` is there to keep Windows Nginx from complaining about missing logs folder
nginx -p "$PWD" -c nginx.conf -t -e stderr
# Try to reload daemon, otherwise spawn new one
nginx -p "$PWD" -s reload -c nginx.conf -e stderr 2>/dev/null || nginx -p "$PWD" -c nginx.conf -e stderr

echo 'Open http://localhost:2280/weblib/pep-sample-client/ in browser'
