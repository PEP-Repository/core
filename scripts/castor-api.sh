#!/usr/bin/env bash

set -o errexit
set -o nounset

keyfile="$1"
endpoint="$2"
outdir="${3:-}"

host="https://data.castoredc.com"
api_root="$host/api"

client_id=$(jq -r '.ClientKey' "$keyfile")
client_secret=$(jq -r '.ClientSecret' "$keyfile")

oauth_json=$(curl --no-progress-meter -X POST "$host/oauth/token" -H "Content-Type: application/x-www-form-urlencoded" -d "grant_type=client_credentials&client_id=$client_id&client_secret=$client_secret")
token=$(echo "$oauth_json" | jq -r ".access_token")

retrieve() {
  url="$1"
  >&2 echo "Sending request to $url"
  json=$(curl --no-progress-meter --header "Authorization: Bearer $token" "$url" | python3 -m json.tool)
  if [[ -z "$outdir" ]]; then
    echo "$json"
  else
    outfile=$(echo "$url" | tr ':' '_' | tr '/' '_' | tr '?' '_') # | sed 's:/:_:g' | tr '?' '_')
    echo "$json" > "$outdir/$outfile.json"
  fi
  
  # Get empty string instead of "null" if node doesn't exist: https://stackoverflow.com/a/56205949
  next=$(echo "$json" | jq -r "._links.next.href // empty")
  if [[ -n "$next" ]]; then
    retrieve "$next"
  fi
}

retrieve "$api_root$endpoint"
