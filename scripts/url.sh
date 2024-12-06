#!/usr/bin/env sh

# URL processing.

set -o errexit
set -o nounset

command="$1"
value="$2"

# URL parsing copied/adapted from https://stackoverflow.com/a/6174447

get_protocol_from_url() {
  url="$1"
  
  echo "$url" | grep :// | sed -e's,^\(.*://\).*,\1,g'
}

drop_proto_from_url() {
  url="$1"
  
  # extract the protocol
  proto=$(get_protocol_from_url "$url")
  # remove the protocol -- updated
  echo "$url" | sed -e s,"$proto",,g
}

get_host_from_url() {
  url=$(drop_proto_from_url "$1")
  
  # extract the user (if any)
  user="$(echo "$url" | grep @ | cut -d@ -f1)"
  # extract the host and port -- updated
  hostport=$(echo "$url" | sed -e s,"$user"@,,g | cut -d/ -f1)
  # by request host without port
  echo "$hostport" | sed -e 's,:.*,,g'
}

get_path_from_url() {
  url=$(drop_proto_from_url "$1")
  
  # extract the path (if any)
  echo "$url" | grep / | cut -d/ -f2-
}

urlencode() {
  raw="$1"
  jq -rn --arg x "${raw}" '$x|@uri'
}

case $command in
  protocol)
    get_protocol_from_url "$value"
    ;;
  host)
    get_host_from_url "$value"
    ;;
  path)
    get_path_from_url "$value"
    ;;
  encode)
    urlencode "$value"
    ;;
  *)
    >&2 echo Unsupported URL command: "$command"
    exit 1
esac
