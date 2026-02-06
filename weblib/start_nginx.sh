#!/usr/bin/env sh
set -eu

# Start Nginx in the background, serving pep-sample-client
# If already running, just updates configuration

scriptdir="$(realpath -- "$(dirname -- "$0")")"

wasm_build_folder="$(realpath -- "${1:-"$scriptdir/../build/wasm32/Debug/"}")"

cd "$scriptdir"

cat >../temp/nginx-variables.conf <<EOF
set \$home "$HOME";
set \$build_folder "$wasm_build_folder";
EOF

nginx -p "$PWD" -c nginx.conf -T >/dev/null
nginx -p "$PWD" -s reload -c nginx.conf || nginx -p "$PWD" -c nginx.conf

echo 'Open http://localhost:2280/weblib/pep-sample-client/ in browser'
