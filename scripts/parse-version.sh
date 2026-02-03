#!/usr/bin/env sh

set -eu

command="$1"
versionfile="$2"

if [ ! -f "$versionfile" ]; then
  >&2 echo "Error reading version file: input file \"$versionfile\" does not exist."
  exit 1
fi


version_json=$(cat "$versionfile")
version_major=$(echo "$version_json" | jq -r '.major // error("no major version value found")')
version_minor=$(echo "$version_json" | jq -r '.minor // error("no minor version value found")')

case $command in
  get-major)
    echo "$version_major"
    ;;
  get-minor)
    echo "$version_minor"
    ;;
  env-var-assignments)
    echo "PEP_VERSION_MAJOR=$version_major"
    echo "PEP_VERSION_MINOR=$version_minor"
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
