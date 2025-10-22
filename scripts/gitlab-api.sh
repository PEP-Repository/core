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

  max_retries=3
  retry_delay=5
  attempt=1
  
  while [ $attempt -le $max_retries ]; do
    set +e  # To be able to check curl exit code
    response=$(curl --no-progress-meter \
                    --request "$method" \
                    --header "PRIVATE-TOKEN: $api_key" \
                    --header "Cache-Control: no-cache" \
                    "$url" "$@")
    curl_exit=$?
    set -e  # Re-enable errexit
    
    if [ $curl_exit -eq 0 ]; then
      echo "$response"
      return 0
    fi

    # Only retry on curl exit code 22 (HTTP error code 400+)
    if [ $curl_exit -eq 22 ]; then
      if [ $attempt -lt $max_retries ]; then
        >&2 echo "Request failed (attempt $attempt/$max_retries), retrying in ${retry_delay}s..."
        sleep $retry_delay
        retry_delay=$((retry_delay * 2))
        attempt=$((attempt + 1))
      else
        >&2 echo "Request failed after $max_retries attempts"
        # Print full response with headers for debugging
        curl --no-progress-meter -i \
          --request "$method" -H "PRIVATE-TOKEN: $api_key" -H "Cache-Control: no-cache" \
          "$url" "$@" >&2 || true
        >&2 echo "Error while sending $method request to $url" "$@"
        return 1
      fi
    else
      >&2 echo "Request failed with curl exit code $curl_exit"
      >&2 echo "Error while sending $method request to $url" "$@"
      return 1
    fi
  done
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
