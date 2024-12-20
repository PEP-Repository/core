#!/usr/bin/env sh

# Extracts fields from a JSON file containing Gitlab API key credentials.

set -o errexit
set -o nounset

field="$1"
file="$2"

case "$field" in
  key|user)
    jq -r ".$field" "$file"
    ;;
  *)
    >&2 echo Unsupported API key field "$field"
    exit 1
    ;;
esac
