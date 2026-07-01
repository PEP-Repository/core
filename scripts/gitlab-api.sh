#!/usr/bin/env sh

# Frontend for the Gitlab API.

# Given a directory containing (a clone of) a Gitlab project, provides access
# to the associated API.

set -eu

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/sh-utils.sh"

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

readonly git_dir="${1:?Expected git dir}"; shift
readonly api_key="${1:?Expected API key}"; shift
readonly command="${1:?Expected command}"; shift

# Commands that don't require a project URL or API connection
case $command in
  get-outdated-creation-timestamp)
    # Read a JSON object from stdin containing a ".created_at" property, e.g.
    # - Docker image (tag) details: https://docs.gitlab.com/ee/api/container_registry.html#get-details-of-a-registry-repository-tag
    # - project package details: https://docs.gitlab.com/ee/api/packages.html#get-a-project-package
    # - pipelines
    # If the ".created_at" is older than the (hard-coded) threshold, this function prints
    # the value of that ".created_at" property (so that the caller can report its value), otherwise print nothing. Exit 0 in both cases.
    entry=$(cat)
    created_at=$(printf '%s' "$entry" | jq --raw-output ".created_at")
    seconds=$(( $(date +%s) - $(date -d "$created_at" +%s) ))
    days=$(( seconds / 60 / 60 / 24 ))
    if [ "$days" -ge 6 ]; then
      echo "$created_at"
    fi
    exit 0
    ;;
esac

rel_path="${1?:Expected URL path}"; shift
# Further arguments are passed verbatim to the "curl" command(s) that we issue

git_root=$(cd "$git_dir" && pwd)
gitlab_host=$("$SCRIPTPATH"/gitdir.sh origin-host "$git_root")
if ! $no_project; then
  project_path=$("$SCRIPTPATH"/gitdir.sh origin-path "$git_root")
  project_id=$("$SCRIPTPATH"/url.sh encode "${project_path}")
fi

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
    # per_page=100 is the maximum allowed, so like this we minimize the number of requests
    page=$(request get "${rel_path}${delim}per_page=100&page=$ipage" "$@")
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
