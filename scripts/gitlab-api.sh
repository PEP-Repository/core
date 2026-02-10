#!/usr/bin/env sh

# Frontend for the Gitlab API.

# Given a directory containing (a clone of) a Gitlab project, provides access
# to the associated API.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

no_project=false
while [ "$#" != 0 ]; do
  case "$1" in
    --no-project) # Do not prefix API URL with project
      no_project=true ;;
    --help|-h)
      echo "Usage: '$0' [--no-project] <git-dir> <api-key> <command> <rel-path> [curl args...]"
      exit ;;
    --)
      shift
      break ;;
    --*)
      >&2 echo "$0: Unknown option: $1"
      exit 2 ;;
    *) break ;;
  esac
  shift
done

git_dir="${1:?Expected git dir}"; shift
api_key="${1:?Expected API key}"; shift
command="${1:?Expected command}"; shift
rel_path="${1?:Expected URL path}"; shift
# Further arguments are passed verbatim to the "curl" command(s) that we issue

git_root=$(cd "$git_dir" && pwd)
gitlab_host=$("$SCRIPTPATH"/gitdir.sh origin-host "$git_root")
if ! $no_project; then
  project_path=$("$SCRIPTPATH"/gitdir.sh origin-path "$git_root")
  project_id=$("$SCRIPTPATH"/url.sh encode "${project_path}")
fi

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

  url="https://$gitlab_host/api/v4/"
  if ! $no_project; then
    url="${url}projects/$project_id/"
  fi
  url="$url$path"

  if [ -n "${DRY_DELETE-}" ] && [ "$method" = DELETE ]; then
    >&2 echo "DELETE $url"
    return
  fi

  if ! curl --no-progress-meter \
            --fail \
            --retry 7 \
            --request "$method" \
            --header "PRIVATE-TOKEN: $api_key" \
            --header "Cache-Control: no-cache" \
            "$url" "$@"; then
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
