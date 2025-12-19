#!/usr/bin/env sh
set -eu

# Start Nginx in the background, serving pep-sample-client

scriptdir="$(realpath "$(dirname -- "$0")")"
cd "$scriptdir"

cat >./dist/nginx_variables.conf <<EOF
set \$home "$HOME";
EOF

nginx -p "$PWD" -c nginx.conf -T >/dev/null
nginx -p "$PWD" -s reload -c nginx.conf || nginx -p "$PWD" -c nginx.conf

echo 'Open http://localhost:2280/weblib/pep-sample-client/ in browser'
