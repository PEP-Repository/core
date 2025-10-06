#!/usr/bin/env sh

# Frontend for the Gitlab API.

# Given a directory containing (a clone of) a Gitlab project, provides access
# to the associated API.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

git_dir="$1"
api_key="$2"
command="$3"
rel_path="$4"
# Further arguments are passed verbatim to the "curl" command(s) that we issue
shift
shift
shift
shift

git_root=$(cd "$git_dir" && pwd)
gitlab_host=$("$SCRIPTPATH"/gitdir.sh origin-host "$git_root")
project_path=$("$SCRIPTPATH"/gitdir.sh origin-path "$git_root")
project_id=$("$SCRIPTPATH"/url.sh encode "${project_path}")

contains() {
  string="$1"
  substring="$2"
  # `&& true` prevents quitting for nonzero exit code
  [ "${string#*"$substring"}" != "$string" ] && true
}

request() {
  method=$(echo "$1" | tr "[:lower:]" "[:upper:]")
  path="$2"
  shift
  shift
  url="https://$gitlab_host/api/v4/projects/$project_id/$path"
  # >&2 echo "Sending $method request to $url" "$@"
  if ! curl --no-progress-meter --fail --request "$method" -H "PRIVATE-TOKEN: $api_key" -H "Cache-Control: no-cache" "$url" "$@"; then
    >&2 echo "Error while sending $method request to $url" "$@"
    return 1
  fi
}

get_multipage() {
  rel_path="$1"
  shift
  
  delim="?"
  if contains "$rel_path" "?"; then
    delim="&"
  fi

  # Use printf '%s' instead of echo to prevent character escapes from being interpreted

  joined=""
  ipage=1
  while [ "$ipage" -ne 0 ]; do
    page=$(request get "${rel_path}${delim}page=$ipage" "$@")
    if [ "$(printf '%s' "$page" | jq length)" -eq 0 ]; then
      ipage=0
    else
      # Join with newline (string escapes are not a thing in sh)
      joined="$joined
$page"
      ipage=$((ipage + 1))
    fi
  done
  
  printf '%s' "$joined" | jq ".[]" | jq -s
}

# TODO: support aNyCaSeCoMmAnDs
case $command in
  get-multipage)
    get_multipage "$rel_path" "$@"
    ;;
  get|post|put|delete)
    request "$command" "$rel_path" "$@"
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
