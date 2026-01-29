#!/usr/bin/env bash
set -eu

# Proxy servers from ClientConfig.json over WebSockets
# Requires websockify (pipx install websockify)

original_host_override="${1-}"

scriptdir="$(realpath -- "$(dirname -- "$0")")"

# Echo without newlines or processed escapes (which some sh implementations do)
raw_echo() {
  printf %s "$*"
}

stop_jobs() {
  jobs="$(jobs -rp)"
  if [ -n "$jobs" ]; then
    # shellcheck disable=SC2086 # Split PIDs
    kill $jobs || true
    # shellcheck disable=SC2086 # Split PIDs
    wait -f $jobs || true
  fi
}
trap stop_jobs EXIT

readarray -t servers < <(jq --compact-output '.[] | select(type == "object" and .Port != null)' "$scriptdir/dist-test/ClientConfig.json")
if [ "${#servers[@]}" -eq 0 ]; then
  >&2 echo "Failed to get addresses to proxy"
  exit 1
fi

for server in "${servers[@]}"; do
  server_name="$(raw_echo "$server" | jq --raw-output .Name)"
  original_host="$(raw_echo "$server" | jq --raw-output .Address)"
  original_port="$(raw_echo "$server" | jq --raw-output .Port)"

  if [ -n "$original_host_override" ] && [ "$original_host_override" != "$original_host" ]; then
    >&2 echo "$server_name proxy: overriding host $original_host with $original_host_override"
    original_host="$original_host_override"
  fi

  ws_port="$((original_port - 1000))"
  websockify "$ws_port" "$original_host:$original_port" 2> >(sed -u "s/^/[websockify $server_name]: /" >&2) > >(sed -u "s/^/[websockify $server_name]: /") &
done

# Wait for first job to exit
wait -fn
