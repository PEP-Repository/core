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
git_dir=''
api_key=''
token_header='PRIVATE-TOKEN'
while [ "$#" != 0 ]; do
  case "$1" in
    --no-project) # Do not prefix API URL with project
      no_project=true ;;
    # Authenticate with a CI job token ($CI_JOB_TOKEN), which is only accepted in its own header:
    # https://docs.gitlab.com/ci/jobs/ci_job_token/#rest-api-authentication
    --job-token)
      shift; api_key="${1:?Expected value for --job-token}"; token_header='JOB-TOKEN' ;;
    --api-key)
      shift; api_key="${1:?Expected value for --api-key}" ;;
    --git-dir)
      shift; git_dir="${1:?Expected value for --git-dir}" ;;
    --help|-h)
      echo "Usage: '$0' [--no-project] --git-dir <dir> (--api-key <key> | --job-token <token>) <command> <rel-path> [curl args...]"
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

readonly git_dir="${git_dir:?Expected --git-dir}"
readonly api_key="${api_key:?Expected --api-key or --job-token}"
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
    created_at=$(raw_echo "$entry" | jq --raw-output ".created_at")
    seconds=$(( $(date +%s) - $(gnu_date -d "$created_at" +%s) ))
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
            --header "$token_header: $api_key" \
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

  joined=""
  ipage=1
  while [ "$ipage" -ne 0 ]; do
    # per_page=100 is the maximum allowed, so like this we minimize the number of requests
    page=$(request get "${rel_path}${delim}per_page=100&page=$ipage" "$@")
    # jq length would fail on anything not a json array, and treating that as a nonempty page would loop over pages forever
    page_length=$(raw_echo "$page" | jq 'if type == "array" then length else null end' 2>/dev/null) || page_length=null
    if [ "$page_length" = null ]; then
      fail "Expected a JSON array from $rel_path (page $ipage), got: $page"
    fi
    if [ "$page_length" -eq 0 ]; then
      ipage=0
    else
      # Join with newline (string escapes are not a thing in sh)
      joined="$joined
$page"
      ipage=$((ipage + 1))
    fi
  done
  
  raw_echo "$joined" | jq ".[]" | jq -s
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
