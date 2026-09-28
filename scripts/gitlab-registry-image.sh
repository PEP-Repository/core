#!/usr/bin/env sh

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

command="$1"
registry_root="$2"
git_dir="$3"

get_location() {
  imgname="$1"
  sha="$2"
  
  rel="$("$SCRIPTPATH/gitdir.sh" origin-path "$git_dir")"
  ref="$registry_root/$rel/$imgname:$sha"
  
  # (Try to) pull the current image so we can `docker image inspect` it
  if ! docker pull "$ref" 1>&2; then # Prevent docker command's stdout from cluttering this function's output
    >&2 echo "Image not found at $ref"
    return
  fi
  
  # Don't echo a location if the image (exists but) is outdated
  created_at="$(docker image inspect "$ref" | jq -r ".[].Created")"
  if ! "$SCRIPTPATH/is-up-to-date.sh" "$created_at"; then
    >&2 echo "Image $ref is outdated: created at $created_at"
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
