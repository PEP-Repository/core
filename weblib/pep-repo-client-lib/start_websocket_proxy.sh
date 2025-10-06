#!/usr/bin/env bash
set -eu

original_host="${1:-localhost}"

# Requires websockify

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

original_ports=(
  16501
  16519
  16511
  16516
  16518
  16512
)
for original_port in "${original_ports[@]}"; do
  ws_port="$((original_port - 1000))"
  websockify "$ws_port" "$original_host:$original_port" 2> >(sed -u "s/^/[websockify $original_port]: /" >&2) > >(sed -u "s/^/[websockify $original_port]: /") &
done

# Wait for first job to exit
wait -fn
