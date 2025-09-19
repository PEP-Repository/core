#!/usr/bin/env sh

set -eu -o noglob

if [ ${#} -lt 1 ]; then
  >&2 echo "Usage: $0 <server> [port=443] [CAfile]"
  exit
fi

server="$1"
port="${2:-443}"
CAOption="${3:+"-CAfile $3"}"

certfile=$(mktemp)

echo | openssl s_client -showcerts -servername "$server" -connect "$server:$port" 2>/dev/null > "$certfile"
# `openssl verify` by default only verifies the first certificate in a file, not the full chain. But you can use `-untrusted` to pass additional certificates it can use to build a chain.
# since $certfile contains the full chain, with the certificate we want to check at the top, we pass it as both the -untrusted parameter and as the certificate to check
# shellcheck disable=SC2086
openssl verify -untrusted "$certfile" $CAOption "$certfile"