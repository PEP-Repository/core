#!/usr/bin/env sh

# Reports properties of a git (source-controlled) directory.

# Scripts some usages of the git client.

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

command="$1"
dir="$2"

git_root=$(cd "$dir" && pwd)

get_origin_url() {
  origin_url=$(cd "$git_root" && git config --get remote.origin.url)
  
  case "$origin_url" in
    *.git) # Remove ".git" extension if present: https://stackoverflow.com/a/27658717
    origin_url=${origin_url%.*}
    ;;
  esac
  
  echo "$origin_url"
}

get_origin_host_and_path() {
  url=$(get_origin_url "$git_root")

  # Differentiate between real (e.g. http) URLs and SSH-ish "user@host:path"
  # in git origin urls. Even though the latter aren't (properly formatted)
  # URLs, invocations of "url.sh protocol" produce an empty string without
  # errors.
  proto=$("$SCRIPTPATH"/url.sh protocol "$url")
  if [ -n "$proto" ]; then
    "$SCRIPTPATH"/url.sh host "$url"
    "$SCRIPTPATH"/url.sh path "$url"
  else
    # Adapted from url.sh
    user="$(echo "$url" | grep @ | cut -d@ -f1)"
    hostcolonpath=$(echo "$url" | sed -e s,"$user"@,,g)
    
    # Check if there's a single colon
    if [ "$(echo "$hostcolonpath" | sed 's/[^:]//g')" != ":" ]; then
      >&2 echo Git origin URL is not a valid 'user@host:remote/path' spec: "$url"
      exit 1
    fi
    # Split host:remote/path on the colon
    echo "$hostcolonpath" | cut -d: -f1
    echo "$hostcolonpath" | cut -d: -f2
  fi
}

get_origin_host() {
  get_origin_host_and_path | sed -n '1 p'
}

get_origin_path() {
  get_origin_host_and_path | sed -n '2 p'
}

get_submodule_dirs() {
  # List submodule directories: https://stackoverflow.com/a/12641787
  cd "$git_root" && git config --file .gitmodules --get-regexp path | awk '{ print $2 }' | while read -r subdir
  do
    echo "$git_root/$subdir"
  done
}

case $command in
  origin-url)
    get_origin_url "$git_root"
    ;;
  origin-host)
    get_origin_host "$git_root"
    ;;
  origin-path)
    get_origin_path "$git_root"
    ;;
  submodule-dirs)
    get_submodule_dirs "$git_root"
    ;;
  commit-sha)
    cd "$git_root" && git rev-parse HEAD
    ;;
  get-branch-name)
    cd "$git_root" && git rev-parse --abbrev-ref HEAD
    ;;
  get-project-root)
    cd "$git_root" && git rev-parse --show-toplevel
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
