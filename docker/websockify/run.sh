#!/usr/bin/env bash
# Proxy an arbitrary set of TCP services over Secure WebSockets, using websockify.
#
# The services to proxy are listed in a config file, with the format:
#   "<listen port> <target host>:<target port>"
# one service per line.

set -eu

usage() {
  echo "Usage: $0 --cert <file> --key <file> [<proxy config>]"
  echo "Options:"
  echo "  --cert <file>   Certificate to serve wss:// with"
  echo "  --key <file>    Private key of that certificate"
  echo "  <proxy config>  The proxies to run (default: /config/proxies.conf)"
}

cert=
key=

while [ "$#" != 0 ]; do
  case "$1" in
    --cert)
      shift; cert="${1:?Expected value for --cert}" ;;
    --key)
      shift; key="${1:?Expected value for --key}" ;;
    --help|-h)
      usage
      exit ;;
    --)
      shift
      break
      ;;
    -*)
      >&2 echo "$0: Unknown option: $1"
      >&2 usage
      exit 2
      ;;
    *)
      break
      ;;
  esac
  shift
done

config="${1:-/config/proxies.conf}"

if [ "$#" -gt 1 ]; then
  >&2 echo "$0: Too many arguments"
  >&2 usage
  exit 2
fi

readonly cert key config

if [ -z "$cert" ] || [ -z "$key" ]; then
  >&2 echo "$0: --cert and --key are required"
  >&2 usage
  exit 2
fi
for file in "$cert" "$key"; do
  if [ ! -r "$file" ]; then
    >&2 echo "$0: Cannot read certificate or key $file"
    exit 1
  fi
done

if [ ! -f "$config" ]; then
  >&2 echo "$0: No proxy configuration at $config"
  exit 1
fi

pids=()

is_blank_or_comment() {
  # if whitespace or starts with #, return true
  [[ "$1" =~ ^[[:space:]]*(#|$) ]]
}

check_proxy() {
  # If not 2 args, or arg1 isnt digits, or arg2 isnt "something:number"
  if [ "$#" != 2 ] || ! [[ "$1" =~ ^[0-9]+$ ]] || ! [[ "$2" =~ ^[^:]+:[0-9]+$ ]]; then
    >&2 echo "$0: Expected '<listen port> <target host>:<target port>' in $config, got: $*"
    exit 1
  fi
}

start_proxy() {
  echo "Serving $2 on port $1"
  websockify --cert="$cert" --key="$key" "$1" "$2" &
  pids+=("$!")
}

start_proxies() {
  local line words
  while IFS= read -r line; do
    if is_blank_or_comment "$line"; then
      continue
    fi
    read -r -a words <<< "$line"
    check_proxy "${words[@]}"
    start_proxy "${words[@]}"
  done < "$config"

  if [ "${#pids[@]}" = 0 ]; then
    >&2 echo "$0: No proxies configured in $config"
    exit 1
  fi
}

# Stop the proxies that are still running, so that none of them outlive this script
stop_proxies() {
  # Makes sure to run this only once
  trap - EXIT INT TERM
  if [ "${#pids[@]}" != 0 ]; then
    kill "${pids[@]}" 2>/dev/null || true
    wait "${pids[@]}" 2>/dev/null || true
  fi
}

trap stop_proxies EXIT INT TERM
start_proxies

wait -n
