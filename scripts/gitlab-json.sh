#!/usr/bin/env sh

# Processes JSON objects returned by the Gitlab API.

set -eu

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/sh-utils.sh"

readonly command="${1:?Expected command}"; shift

# Read a JSON object from stdin containing a ".created_at" property, e.g.
# - Docker image (tag) details: https://docs.gitlab.com/ee/api/container_registry.html#get-details-of-a-registry-repository-tag
# - project package details: https://docs.gitlab.com/ee/api/packages.html#get-a-project-package
# - pipelines
# If the ".created_at" is older than the (hard-coded) threshold, this function prints
# the value of that ".created_at" property (so that the caller can report its value), otherwise print nothing. Exit 0 in both cases.
get_outdated_creation_timestamp() {
  entry=$(cat)
  created_at=$(raw_echo "$entry" | jq --raw-output ".created_at")
  seconds=$(( $(gnu_date +%s) - $(gnu_date -d "$created_at" +%s) ))
  days=$(( seconds / 60 / 60 / 24 ))
  if [ "$days" -ge 6 ]; then
    echo "$created_at"
  fi
}

case $command in
  get-outdated-creation-timestamp)
    get_outdated_creation_timestamp
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    exit 1
    ;;
esac
