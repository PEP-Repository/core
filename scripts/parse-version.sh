#!/usr/bin/env sh

set -eu

command="$1"
versionfile="$2"
pipeline="$3"
job="$4"

if [ ! -f "$versionfile" ]; then
  >&2 echo "Error reading version file: input file \"$versionfile\" does not exist."
  exit 1
fi


version_json=$(cat "$versionfile")
version_major=$(echo "$version_json" | jq -r '.major // error("no major version value found")')
version_minor=$(echo "$version_json" | jq -r '.minor // error("no minor version value found")')
version_build_offset=$(echo "$version_json" | jq -r '.build_offset // 0')

get_build() {
  if [ "$1" -lt "$version_build_offset" ];
  then
    echo "$1"
  else
    echo $(($1 - $version_build_offset))
  fi
}

case $command in
  get-major)
    echo "$version_major"
    ;;
  get-minor)
    echo "$version_minor"
    ;;
  get-build)
    get_build "$pipeline"
    ;;
  get-revision)
    echo "$job"
    ;;
  env-var-assignments)
    echo "PEP_VERSION_MAJOR=$version_major"
    echo "PEP_VERSION_MINOR=$version_minor"
    echo "PEP_VERSION_BUILD=$(get_build "$3")"
    # Don't write PEP_VERSION_REVISION: we never want/need the job ID of the "load-version" CI job,
    # which is the only location where this command is invoked.
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
