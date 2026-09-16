#!/usr/bin/env sh

# Returns nonzero if the "timestamp" parameter is older than the (hard-coded) threshold.

set -eu

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/sh-utils.sh"

readonly timestamp="${1:?Expected timestamp}"; shift

seconds=$(( $(gnu_date +%s) - $(gnu_date -d "$timestamp" +%s) ))
days=$(( seconds / 60 / 60 / 24 ))
if [ "$days" -ge 6 ]; then
  exit 1
fi
