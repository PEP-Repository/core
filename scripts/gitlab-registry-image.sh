#!/usr/bin/env sh

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

. "$SCRIPTPATH/sh-utils.sh"

command="$1"
registry_root="$2"
git_dir="$3"

# TODO: consolidate duplicate code with gitlab-json.sh
get_outdated_creation_timestamp() {
  created_at="$1"
  seconds=$(( $(gnu_date +%s) - $(gnu_date -d "$created_at" +%s) ))
  days=$(( seconds / 60 / 60 / 24 ))
  if [ "$days" -ge 6 ]; then
    echo "$created_at"
  fi
}

get_location() {
  imgname="$1"
  sha="$2"
  
  rel="$("$SCRIPTPATH/gitdir.sh" origin-path "$git_dir")"
  ref="$registry_root/$rel/$imgname:$sha"
  
  # (Try to) pull the current image so we can `docker image inspect` it
  if ! docker pull "$ref" > /dev/null 2> /dev/null; then
    >&2 echo "Image not found at $ref"
	return
  fi
  
  # Don't echo a location if the image (exists but) is outdated
  created="$(docker image inspect "$ref" | jq -r ".[].Created")"
  if [ -n "$(get_outdated_creation_timestamp "$created")" ]; then
    >&2 echo "Image $ref is outdated: created at $created"
	return
  fi
  
  # Image (exists and) is recent enough: echo its ref
  echo "$ref"
}

# TODO: support aNyCaSeCoMmAnDs
case $command in
  get-location)
    get_location "$4" "$5"
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
